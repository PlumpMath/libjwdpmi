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

// Hardware exception handling functionality.

#pragma once

#include <iostream>
#include <cstdint>
#include <algorithm>
#include <deque>
#include <stdexcept>
#include <bitset>
#include <string>
#include <function.h>
#include <jw/enum_struct.h>
#include <jw/dpmi/dpmi.h>
#include <jw/dpmi/lock.h>
#include <jw/dpmi/alloc.h>
#include <jw/dpmi/irq_check.h>
#include <jw/dpmi/fpu.h>
#include <../jwdpmi_config.h>

namespace jw
{
    namespace dpmi
    {
        struct [[gnu::packed]] old_exception_frame
        {
            far_ptr32 return_address; unsigned : 16;
            std::uint32_t error_code;
            far_ptr32 fault_address;
            union [[gnu::packed]]
            {
                struct[[gnu::packed]] // DPMI 1.0 only
                {
                    bool host_exception : 1;
                    bool cannot_retry : 1;
                    bool redirect_elsewhere : 1;
                    unsigned : 13;
                };
                std::uint16_t raw_info_bits;
            } info_bits;
            union [[gnu::packed]]
            {
                struct[[gnu::packed]]
                {
                    bool carry : 1;
                    unsigned : 1;
                    bool parity : 1;
                    unsigned : 1;
                    bool adjust : 1;
                    unsigned : 1;
                    bool zero : 1;
                    bool sign : 1;
                    bool trap : 1;
                    bool interrupt : 1;
                    bool direction : 1;
                    bool overflow : 1;
                    unsigned iopl : 2;
                    bool nested_task : 1;
                    unsigned : 1;
                    bool resume : 1;
                    bool v86mode : 1;
                    bool alignment_check : 1;
                    bool virtual_interrupt : 1;
                    bool virtual_interrupt_pending : 1;
                    bool cpuid_available : 1;
                    unsigned : 10;
                };
                std::uint32_t raw_eflags;
            } flags;
            far_ptr32 stack; unsigned : 16;

            auto& print(auto& out) const
            {
                using namespace std;
                out << hex << setfill('0');
                out << "CPU exception at cs:eip=" << setw(4) << fault_address.segment << ':' << setw(8) << fault_address.offset;
                out << ", ss:esp=" << setw(4) << stack.segment << ':' << setw(8) << stack.offset << '\n';
                out << "Error code: " << setw(8) << error_code;
                out << ", Info bits: " << std::bitset<3>(info_bits.raw_info_bits);
                out << ", Flags: " << std::bitset<22>(flags.raw_eflags) << '\n';
                out << setfill(' ') << setw(0);
                return out;
            }
            friend auto& operator<<(std::ostream& out, const old_exception_frame& in) { return in.print(out); }
        };
        struct[[gnu::packed]] new_exception_frame : public old_exception_frame
        {
            selector es; unsigned : 16;
            selector ds; unsigned : 16;
            selector fs; unsigned : 16;
            selector gs; unsigned : 16;
            std::uintptr_t linear_page_fault_address : 32;
            union
            {
                struct[[gnu::packed]]
                {
                    bool present : 1;
                    bool write_access : 1;
                    bool user_access : 1;
                    bool write_through : 1;
                    bool cache_disabled : 1;
                    bool accessed : 1;
                    bool dirty : 1;
                    bool global : 1;
                    unsigned reserved : 3;
                    unsigned physical_address : 21;
                };
                unsigned raw_pte : 32;
            } page_table_entry;

            auto& print(auto& out) const
            {
                out << static_cast<old_exception_frame>(*this);
                using namespace std;
                out << hex << setfill('0');
                out << "(if page fault) Linear: " << setw(8) << linear_page_fault_address << ", Physical: " << setw(8) << page_table_entry.physical_address;
                out << ", PTE: " << std::bitset<8>(page_table_entry.raw_pte) << '\n';
                out << "ds=" << setw(4) << ds << " es=" << setw(4) << es << " fs=" << setw(4) << fs << " gs=" << setw(4) << gs << '\n';
                out << setfill(' ') << setw(0);
                return out;
            }
            friend auto& operator<<(std::ostream& out, const new_exception_frame& in) { return in.print(out); }
        };

        struct[[gnu::packed]] raw_exception_frame
        {
            cpu_registers reg;
            old_exception_frame frame_09;
            new_exception_frame frame_10;
        };

        static_assert(sizeof(old_exception_frame) == 0x20, "check sizeof old_exception_frame");
        static_assert(sizeof(new_exception_frame) == 0x38, "check sizeof new_exception_frame");
        static_assert(sizeof(raw_exception_frame) == 0x78, "check sizeof raw_exception_frame");

        using exception_frame = old_exception_frame; // can be static_cast to new_exception_frame type
        using exception_handler_sig = bool(cpu_registers*, exception_frame*, bool);

        struct exception_num : public enum_struct<std::uint32_t>
        {
            using E = enum_struct<std::uint32_t>;
            using T = typename E::underlying_type;
            enum
            {
                divide_error = 0,
                debug,
                non_maskable_interrupt,
                breakpoint,
                overflow,
                bound_range_exceeded,
                invalid_opcode,
                device_not_available,
                double_fault,
                x87_segment_not_present,
                invalid_tss,
                segment_not_present,
                stack_segment_fault,
                general_protection_fault,
                page_fault,
                x87_exception = 0x10,
                alignment_check,
                machine_check,
                sse_exception,
                virtualization_exception,
                security_exception = 0x1e
            }; 
            using E::E;
            using E::operator=;
        };
    }
}

#include <jw/dpmi/detail/cpu_exception.h>

namespace jw
{
    namespace dpmi
    {
        class exception_handler : class_lock<exception_handler>
        {
            void init_code();
            func::function<exception_handler_sig> handler;
            exception_num exc;
            exception_handler* next { nullptr };
            exception_handler* prev { nullptr };
            static std::array<exception_handler*, 0x20> last;
            static std::array<byte, config::exception_stack_size> stack; // TODO: allow nested exceptions

            static bool call_handler(exception_handler* self, raw_exception_frame* frame) noexcept;
                                                                // sizeof   alignof     offset
            exception_handler* self { this };                   // 4        4           [eax-0x28]
            decltype(&call_handler) call_ptr { &call_handler }; // 4        4           [eax-0x24]
            byte* stack_ptr;                                    // 4        4           [eax-0x20]
            selector ds;                                        // 2        2           [eax-0x1C]
            selector es;                                        // 2        2           [eax-0x1A]
            selector fs;                                        // 2        2           [eax-0x18]
            selector gs;                                        // 2        2           [eax-0x16]
            bool new_type;                                      // 1        1           [eax-0x14]
            byte _padding;                                      // 1        1           [eax-0x13]
            far_ptr32 chain_to;                                 // 6        2           [eax-0x12]
            std::array<byte, 0x100> code;                       //          1           [eax-0x0C]

        public:
            template<typename F>    // TODO: real-mode (requires a separate wrapper list)
            exception_handler(exception_num e, F&& f, bool = false)
                : handler(std::allocator_arg, locking_allocator<> { }, std::forward<F>(f))
                , exc(e), stack_ptr(stack.data() + stack.size() - 4)
            {
                detail::setup_exception_throwers();
                init_code();

                prev = last[e];
                if (prev != nullptr)prev->next = this;
                last[e] = this;

                chain_to = detail::cpu_exception_handlers::get_pm_handler(e);
                new_type = detail::cpu_exception_handlers::set_pm_handler(e, get_ptr());
            }

            ~exception_handler();

            exception_handler(const exception_handler&) = delete;
            exception_handler(exception_handler&&) = delete;           
            exception_handler& operator=(const exception_handler&) = delete;
            exception_handler& operator=(exception_handler&&) = delete;

            far_ptr32 get_ptr() const noexcept { return far_ptr32 { get_cs(), reinterpret_cast<std::uintptr_t>(code.data()) }; }
        }; 

        struct cpu_category : public std::error_category
        {
            virtual const char* name() const noexcept override { return "CPU"; }
            virtual std::string message(int ev) const override;
        };

        struct cpu_exception : public std::system_error
        {
            cpu_exception(exception_num n) : system_error(n, cpu_category { }) { }
            cpu_exception(exception_num n, const std::string& msg) : system_error(n, cpu_category { }, msg) { }
        };
    }
}
