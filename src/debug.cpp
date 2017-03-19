/******************************* libjwdpmi **********************************
Copyright (C) 2016-2017  J.W. Jagersma

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <array>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <jw/dpmi/fpu.h>
#include <jw/dpmi/dpmi.h>
#include <jw/dpmi/debug.h>
#include <jw/dpmi/cpu_exception.h>
#include <jw/io/rs232.h>
#include <jw/alloc.h>

namespace jw
{
    namespace dpmi
    {
        namespace detail
        {
            bool gdb_interface_setup { false };
        }

        namespace gdb
        {
            locked_pool_allocator<> alloc { 1_MB };
            std::deque<std::string, locked_pool_allocator<>> sent { alloc };
            std::unordered_map<std::string, std::string, std::hash<std::string>, std::equal_to<std::string>, locked_pool_allocator<>> supported { alloc };

            std::array<std::unique_ptr<exception_handler>, 0x20> exception_handlers;
            auto gdb { init_unique<std::iostream>(alloc) };

            enum regnum
            {
                eax, ecx, edx, ebx,
                esp, ebp, esi, edi,
                eip, eflags,
                cs, ss, ds, es, fs, gs,
                st0, st1, st2, st3, st4, st5, st6, st7,
                fctrl, cstat, ftag, fiseg, fioff, foseg, fooff, fop,
                xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7,
                mxcsr
            };

            inline auto signal_number(exception_num exc)
            {
                switch (exc)
                {
                case 0x00: return 0x08; // SIGFPE
                case 0x01: return 0x05; // SIGTRAP
                case 0x02: return 0x09; // SIGKILL
                case 0x03: return 0x05; // SIGTRAP
                case 0x04: return 0x08; // SIGFPE
                case 0x05: return 0x0b; // SIGSEGV
                case 0x06: return 0x04; // SIGILL
                case 0x07: return 0x08; // SIGFPE
                case 0x08: return 0x09; // SIGKILL
                case 0x09: return 0x0b; // SIGSEGV
                case 0x0a: return 0x0b; // SIGSEGV
                case 0x0b: return 0x0b; // SIGSEGV
                case 0x0c: return 0x0b; // SIGSEGV
                case 0x0d: return 0x0b; // SIGSEGV
                case 0x0e: return 0x0b; // SIGSEGV
                case 0x10: return 0x07; // SIGEMT
                case 0x11: return 0x0a; // SIGBUS
                case 0x12: return 0x09; // SIGKILL
                case 0x13: return 0x08; // SIGFPE
                default: return 143;
                }
            }

            std::uint32_t checksum(const std::string& s)
            {
                std::uint8_t r { 0 };
                for (auto c : s) r += c;
                return r;
            }
            
            void send_packet(const std::string& output)
            {
                std::clog << "send --> \"" << output << "\"\n";
                const auto sum = checksum(output);
                *gdb << '$' << output << '#' << std::setfill('0') << std::hex << std::setw(2) << sum << std::flush;
                sent.push_back(output);
            }

            auto recv_packet()
            {                                             
            retry:
                switch (gdb->get())
                {
                case '-': send_packet(sent.front());
                case '+': if (sent.size() > 0) sent.pop_front();
                default: goto retry;
                case '$': break;
                }

                std::string input;
                std::getline(*gdb, input, '#');
                std::string sum;
                sum += gdb->get();
                sum += gdb->get();
                if (std::stoul(sum, nullptr, 0x10) == checksum(input)) *gdb << '+' << std::flush;
                else { *gdb << '-' << std::flush; goto retry; }
                std::clog << "recv <-- \""<< input << "\"\n";

                std::deque<std::string> parsed_input { };
                std::size_t pos { 1 };
                parsed_input.push_back(input.substr(0, 1));
                while (pos < input.size())
                {
                    auto p = std::min({ input.find(',', pos), input.find(':', pos), input.find(';', pos) });
                    if (p == input.npos) p = input.size();
                    parsed_input.push_back(input.substr(pos, p - pos));
                    pos += p - pos + 1;
                }
                return parsed_input;
            }

            template <typename T>
            void reverse(std::ostream& out, T* in, std::size_t len = sizeof(T))
            {
                auto ptr = reinterpret_cast<byte*>(in);
                for (std::size_t i = 0; i < len; ++i) 
                    out << std::setw(2) << static_cast<std::uint32_t>(ptr[i]);
            }

            void reg(std::ostream& out, regnum r, cpu_registers* reg, exception_frame* frame, bool new_type)
            {
                using namespace std;
                auto* new_frame = static_cast<new_exception_frame*>(frame);
                switch (r)
                {
                case eax: reverse(out, &reg->eax); return;
                case ebx: reverse(out, &reg->ebx); return;
                case ecx: reverse(out, &reg->ecx); return;
                case edx: reverse(out, &reg->edx); return;
                case ebp: reverse(out, &reg->ebp); return;
                case esi: reverse(out, &reg->esi); return;
                case edi: reverse(out, &reg->edi); return;
                case esp: reverse(out, &frame->stack.offset); return;
                case eip: reverse(out, &frame->fault_address.offset); return;
                case eflags: reverse(out, &frame->flags.raw_eflags); return;
                case cs: reverse(out, &frame->fault_address.segment); return;
                case ss: reverse(out, &frame->stack.segment); return;
                case ds: if (new_type) reverse(out, &new_frame->ds); return;
                case es: if (new_type) reverse(out, &new_frame->es); return;
                case fs: if (new_type) reverse(out, &new_frame->fs); return;
                case gs: if (new_type) reverse(out, &new_frame->gs); return;
                default: if (r > mxcsr) return;
                    auto fpu = detail::fpu_context_switcher.get_last_context();
                    switch (r)
                    {
                        
                    }
                }
            }
            bool trace { false };
            std::atomic_flag reentry { false };

            bool handle_packet(exception_num exc, cpu_registers* r, exception_frame* f, bool t)
            {
                using namespace std;
                std::deque<std::string> packet { "?" };
                while (true)
                {
                    std::stringstream s { };
                    s << hex << setfill('0');
                    if (!trace) packet = recv_packet();
                    auto& p = packet.front();
                    if (p == "?")
                    {
                        if (exc == 1 || exc == 3)
                        {
                            s << "T" << setw(2) << signal_number(exc);
                            s << eip << ':'; reg(s, eip, r, f, t); s << ';';
                            s << esp << ':'; reg(s, esp, r, f, t); s << ';';
                            s << ebp << ':'; reg(s, ebp, r, f, t); s << ';';
                            s << eflags << ':'; reg(s, eflags, r, f, t); s << ';';
                            s << eax << ':'; reg(s, eax, r, f, t); s << ';';
                            s << ebx << ':'; reg(s, ebx, r, f, t); s << ';';
                            s << ecx << ':'; reg(s, ecx, r, f, t); s << ';';
                            s << edx << ':'; reg(s, edx, r, f, t); s << ';';
                            s << "swbreak:;";
                            send_packet(s.str());
                        }
                        else
                        {
                            s << "S" << setw(2) << signal_number(exc);
                            send_packet(s.str());
                        }
                        trace = false;
                    }
                    else if (p == "q")
                    {
                        auto& q = packet[1];
                        if (q == "Supported")
                        {
                            packet.pop_front();
                            for (auto str : packet)
                            {
                                auto back = str.back();
                                auto equals_sign = str.find('=', 0);
                                if (back == '+' || back == '-')
                                {
                                    str.pop_back();
                                    supported[str] = back;
                                }
                                else if (equals_sign != str.npos)
                                {
                                    supported[str.substr(0, equals_sign)] = str.substr(equals_sign + 1);
                                }
                            }
                            send_packet("PacketSize=100000;swbreak+");
                        }
                        else if (q == "Attached") send_packet("0");
                        else send_packet("");
                    }
                    else if (p == "p")
                    {
                        reg(s, static_cast<regnum>(std::stoul(packet[1], nullptr, 16)), r, f, t);
                        if (s.peek() != EOF) send_packet(s.str());
                        else send_packet("E00");
                    }
                    else if (p == "g")
                    {
                        for (int i = eax; i <= eflags; ++i)
                            reg(s, static_cast<regnum>(i), r, f, t);
                        send_packet(s.str());
                    }
                    else if (p == "G")
                    {
                        send_packet("");    // TODO
                    }
                    else if (p == "m")
                    {
                        auto* addr = reinterpret_cast<byte*>(std::stoul(packet[1], nullptr, 16));
                        std::size_t len = std::stoul(packet[2], nullptr, 16);
                        for (auto i = addr; i < addr + len; ++i) s << setw(2) << static_cast<std::uint32_t>(*i);
                        send_packet(s.str());
                    }
                    else if (p == "M")
                    {
                        auto* addr = reinterpret_cast<byte*>(std::stoul(packet[1], nullptr, 16));
                        std::size_t len = std::stoul(packet[2], nullptr, 16);
                        for (std::size_t i = 0; i < len; ++i)
                            addr[i] = std::stoul(packet[3].substr(i * 2, 2), nullptr, 16);
                        send_packet("OK");
                    }
                    else if (p == "c")
                    {
                        trace = true;
                        f->flags.trap = false;
                        return true;
                    }
                    else if (p == "s")
                    {
                        trace = true;
                        f->flags.trap = true;
                        return true;
                    }
                    else if (p == "k") return false;
                    else send_packet("");
                }
            }

            bool handle_exception(exception_num exc, cpu_registers* r, exception_frame* f, bool t)
            {
                if (reentry.test_and_set()) send_packet("EFF"); // last command caused another exception
                bool result { false };
                try
                {
                    result = handle_packet(exc, r, f, t);
                }
                catch (...) { std::cerr << "Exception occured while communicating with GDB.\n"; }
                reentry.clear();
                return result;
            }

            void setup(const io::rs232_config& cfg)
            {
                if (jw::dpmi::detail::gdb_interface_setup) return;
                jw::dpmi::detail::gdb_interface_setup = true;

                gdb = allocate_unique<io::rs232_stream>(alloc, cfg);

                exception_handlers[0x00] = std::make_unique<exception_handler>(0x00, [](auto* r, auto* f, bool t) { return handle_exception(0x00, r, f, t); });
                exception_handlers[0x01] = std::make_unique<exception_handler>(0x01, [](auto* r, auto* f, bool t) { return handle_exception(0x01, r, f, t); });
                exception_handlers[0x02] = std::make_unique<exception_handler>(0x02, [](auto* r, auto* f, bool t) { return handle_exception(0x02, r, f, t); });
                exception_handlers[0x03] = std::make_unique<exception_handler>(0x03, [](auto* r, auto* f, bool t) { return handle_exception(0x03, r, f, t); });
                exception_handlers[0x04] = std::make_unique<exception_handler>(0x04, [](auto* r, auto* f, bool t) { return handle_exception(0x04, r, f, t); });
                exception_handlers[0x05] = std::make_unique<exception_handler>(0x05, [](auto* r, auto* f, bool t) { return handle_exception(0x05, r, f, t); });
                exception_handlers[0x06] = std::make_unique<exception_handler>(0x06, [](auto* r, auto* f, bool t) { return handle_exception(0x06, r, f, t); });
                exception_handlers[0x07] = std::make_unique<exception_handler>(0x07, [](auto* r, auto* f, bool t) { return handle_exception(0x07, r, f, t); });
                exception_handlers[0x08] = std::make_unique<exception_handler>(0x08, [](auto* r, auto* f, bool t) { return handle_exception(0x08, r, f, t); });
                exception_handlers[0x09] = std::make_unique<exception_handler>(0x09, [](auto* r, auto* f, bool t) { return handle_exception(0x09, r, f, t); });
                exception_handlers[0x0a] = std::make_unique<exception_handler>(0x0a, [](auto* r, auto* f, bool t) { return handle_exception(0x0a, r, f, t); });
                exception_handlers[0x0b] = std::make_unique<exception_handler>(0x0b, [](auto* r, auto* f, bool t) { return handle_exception(0x0b, r, f, t); });
                exception_handlers[0x0c] = std::make_unique<exception_handler>(0x0c, [](auto* r, auto* f, bool t) { return handle_exception(0x0c, r, f, t); });
                exception_handlers[0x0d] = std::make_unique<exception_handler>(0x0d, [](auto* r, auto* f, bool t) { return handle_exception(0x0d, r, f, t); });
                exception_handlers[0x0e] = std::make_unique<exception_handler>(0x0e, [](auto* r, auto* f, bool t) { return handle_exception(0x0e, r, f, t); });

                capabilities c { };
                if (!c.supported) return;
                if (std::strncmp(c.vendor_info.name, "HDPMI", 5) != 0) return;  // TODO: figure out if other hosts support these too
                if (!detail::test_cr0_access())
                    exception_handlers[0x10] = std::make_unique<exception_handler>(0x10, [](auto* r, auto* f, bool t) { return handle_exception(0x10, r, f, t); });
                exception_handlers[0x11] = std::make_unique<exception_handler>(0x11, [](auto* r, auto* f, bool t) { return handle_exception(0x11, r, f, t); });
                exception_handlers[0x12] = std::make_unique<exception_handler>(0x12, [](auto* r, auto* f, bool t) { return handle_exception(0x12, r, f, t); });
                exception_handlers[0x13] = std::make_unique<exception_handler>(0x13, [](auto* r, auto* f, bool t) { return handle_exception(0x13, r, f, t); });
                exception_handlers[0x14] = std::make_unique<exception_handler>(0x14, [](auto* r, auto* f, bool t) { return handle_exception(0x14, r, f, t); });
                exception_handlers[0x1e] = std::make_unique<exception_handler>(0x1e, [](auto* r, auto* f, bool t) { return handle_exception(0x1e, r, f, t); });
            }
        }

        namespace detail
        {
            void setup_gdb_interface(const io::rs232_config& cfg) { jw::dpmi::gdb::setup(cfg); }
        }

        bool debug() { return detail::gdb_interface_setup; }
    } 
}
