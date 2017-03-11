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
#include <unordered_map>
#include <memory>

#include <jw/io/key.h>
#include <jw/io/scancode.h>
#include <jw/io/kb_interface.h>
#include <jw/event.h>

//TODO: keys_changed event with vector of all changed keys
namespace jw
{
    namespace io
    {
        class keyboard final
        {
        public:
            event<void(keyboard&, key_state_pair)> key_changed;

            const key_state& get(key k) { return keys[k]; }
            const key_state& operator[](key k) { return keys[k]; }

            void update();

            keyboard(std::unique_ptr<keyboard_interface>&& intf);

        private:
            std::unique_ptr<keyboard_interface> interface;
            std::unordered_map<key, key_state> keys;
            //static std::unordered_map<key, timer> key_repeat; //TODO: soft typematic repeat
        };
    }
}
