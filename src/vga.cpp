/* * * * * * * * * * * * * * libjwdpmi * * * * * * * * * * * * * */
/* Copyright (C) 2017 J.W. Jagersma, see COPYING.txt for details */

#include <jw/video/vga.h>
#include <jw/dpmi/realmode.h>

namespace jw
{
    namespace video
    {
        constexpr io::in_port<bool>  vga::dac_state;
        constexpr io::io_port<byte>  vga::dac_write_index;
        constexpr io::out_port<byte> vga::dac_read_index;
        constexpr io::io_port<byte>  vga::dac_data;

        void bios::set_mode(vbe_mode m, const crtc_info *)
        {
            dpmi::realmode_registers reg { };
            reg.ah = 0x00;
            reg.al = m.mode;
            reg.call_int(0x10);
        }

        void vga::set_palette_data(std::vector<pixel_bgra>::const_iterator begin, std::vector<pixel_bgra>::const_iterator end, std::uint8_t first, bool)
        {
            dac_write_index.write(first);
            for (auto i = begin; i < end; ++i)
            {
                dac_data.write(i->r);
                dac_data.write(i->g);
                dac_data.write(i->b);
            }
        }
    }
}
