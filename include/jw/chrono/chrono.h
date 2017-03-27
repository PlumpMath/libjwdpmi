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
#include <chrono>
#include <atomic>
#include <deque>
#include <jw/dpmi/irq.h>

namespace jw
{
    namespace chrono
    {
        inline std::uint64_t rdtsc() noexcept
        {
            std::uint64_t tsc;
            asm("rdtsc;": "=A" (tsc));
            return tsc;
        }

        struct chrono
        {
            friend class rtc;
            friend class pit;
            friend class tsc;

            static constexpr long double max_pit_frequency { 1194375.0L / 1.001L };     // freq = max_pit_frequency / divider
            static constexpr std::uint32_t max_rtc_frequency { 0x8000 };                // freq = max_rtc_frequency >> (shift - 1)

            static void setup_pit(bool enable, std::uint32_t freq_divider = 0x1000);    // default: 18.2Hz
            static void setup_rtc(bool enable, std::uint8_t freq_shift = 10);           // default: 64Hz
            static void setup_tsc(std::size_t num_samples = 10);

        private:
            static std::atomic<std::uint64_t> ps_per_tsc_tick;
            static std::uint64_t ps_per_pit_tick;
            static std::uint64_t ps_per_rtc_tick;

            static std::atomic<std::uint64_t> pit_ticks;
            static std::uint_fast16_t rtc_ticks;
            
            static dpmi::irq_handler pit_irq;
            static dpmi::irq_handler rtc_irq;

            static void update_tsc();
            static void reset_pit();
            static void reset_rtc();
            static void reset_tsc();

            static constexpr io::out_port<byte> rtc_index { 0x70 };
            static constexpr io::io_port<byte> rtc_data { 0x71 };

            struct reset_all { ~reset_all(); } static reset;
        };

        struct rtc  // Real-Time Clock
        {
            using duration = std::chrono::microseconds;
            using rep = duration::rep;
            using period = duration::period;
            using time_point = std::chrono::time_point<rtc>;

            static constexpr bool is_steady { false };
            static time_point now() noexcept;

            static std::time_t to_time_t(const time_point& t) noexcept
            {
                return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
            }

            static time_point from_time_t(std::time_t t) noexcept
            {
                return time_point { std::chrono::duration_cast<duration>(std::chrono::seconds { t }) };
            }
        };

        struct pit  // Programmable Interval Timer
        {
            using duration = std::chrono::nanoseconds;
            using rep = duration::rep;
            using period = duration::period;
            using time_point = std::chrono::time_point<pit>;

            static constexpr bool is_steady { false };
            static time_point now() noexcept
            {
                if (!chrono::pit_irq.is_enabled())
                {
                    auto t = std::chrono::duration_cast<duration>(std::chrono::steady_clock::now().time_since_epoch());
                    return time_point { t };
                }
                return time_point { duration { chrono::ps_per_pit_tick * chrono::pit_ticks / 1000 } };
            }
        };

        struct tsc  // Time Stamp Counter
        {
            using duration = std::chrono::nanoseconds;
            using rep = duration::rep;
            using period = duration::period;
            using time_point = std::chrono::time_point<tsc>;

            static constexpr bool is_steady { false };
            static time_point now() noexcept
            {
                if (!chrono::rtc_irq.is_enabled() && !chrono::pit_irq.is_enabled())
                {
                    auto t = std::chrono::duration_cast<duration>(std::chrono::high_resolution_clock::now().time_since_epoch());
                    return time_point { t };
                }
                return time_point { duration { chrono::ps_per_tsc_tick * rdtsc() / 1000 } };
            }
        };
    }
}
