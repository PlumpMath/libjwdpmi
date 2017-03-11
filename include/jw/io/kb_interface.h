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

#pragma once
#include <deque>
#include <jw/typedef.h>
#include <jw/io/scancode.h>

namespace jw
{
    namespace io
    {
        enum leds : byte
        {
            scroll_lock_led = 0b001,
            num_lock_led = 0b010,
            caps_lock_led = 0b100
        };

        enum kb_response
        {
            ACK = 0xFA,
            RESEND = 0xFE,
            ERROR = 0xFC
        };

        class keyboard_interface
        {
        public:
            virtual std::deque<scancode> get_scancodes() = 0;

            virtual scancode_set get_scancode_set() = 0;
            virtual void set_scancode_set(byte set) = 0;

            virtual void set_typematic(byte rate, byte delay) = 0;
            virtual void enable_typematic(bool enable) = 0;

            virtual void set_leds(leds state) = 0;
            virtual void set_leds(bool num, bool caps, bool scroll)
            {
                set_leds(static_cast<leds>(
                    (num ? leds::num_lock_led : 0) |
                    (caps ? leds::caps_lock_led : 0) |
                    (scroll ? leds::scroll_lock_led : 0)));
            }

            virtual ~keyboard_interface() { }
        };
    }
}