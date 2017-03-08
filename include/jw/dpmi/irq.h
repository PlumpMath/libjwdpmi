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

// Interrupt handling functionality.

#pragma once
#include <function.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <deque>
#include <algorithm>
#include <bitset>
#include <jw/io/ioport.h>
#include <jw/dpmi/alloc.h>
#include <jw/dpmi/irq_check.h>
#include <jw/typedef.h>
#include <../jwdpmi_config.h>

// --- --- --- Some notes on DPMI host behaviour: --- --- --- //
// Default RM handlers for INT 0x1C, 0x23, 0x24, and all IRQs reflect to PM, if a PM handler is installed.
// Default PM handlers for all interrupts reflect to RM.

// --- Nested interrupts:
// CWSDPMI: switches to its locked stack on the first interrupt, a nested interrupt calls 
// the handler on the current stack (which should already be locked).
// When a hardware exception occurs and interrupts nest 5 levels deep, it crashes? (exphdlr.c:306)

// HDPMI: does have a "locked" stack (LPMS), not sure why. It doesn't even support virtual memory.
// Also switches to the locked stack only on first interrupt, just like CWSDPMI.

// --- Precautions:
// Lock all static code and data with _CRT0_FLAG_LOCK_MEMORY.
// Lock dynamically allocated memory with dpmi::class_lock or dpmi::data_lock.
// For STL containers, use dpmi::locking_allocator or dpmi::locked_pool_allocator.

// --- When an interrupt occurs:
// Do not allocate any memory. May cause page faults, and malloc() is not re-entrant (or so I hear)
// Do not insert or remove elements in STL containers, which may cause allocation.
// Avoid writing to cout / cerr unless a serious error occurs. INT 21 is not re-entrant.
// Do not use floating point operations. The FPU state is undefined and hardware exceptions may occur.
//      Problem here: there's no telling what the compiler will do.
//      TODO: Possible workaround is to save/restore the entire FPU state and mask all exceptions when an interrupt occurs.
//      TODO: Maybe it's possible to do this lazily by setting TS bit in CR0 and trapping CPU exception 07.

// TODO: (eventually) software interrupts, real-mode callbacks
// TODO: launch threads from interrupts


#define INTERRUPT [[gnu::hot, gnu::optimize("O3"), gnu::used]]

namespace jw
{
    namespace dpmi
    {
        using int_vector = std::uint32_t; // easier to use from asm
        using ack_ptr = void(*)();

        // Configuration flags passed to irq_handler constructor.
        enum irq_config_flags
        {
            // Always call this handler, even if the interrupt has already been acknowledged by a previous handler in the chain.
            always_call = 0b1,

            // Always chain to the default handler (usually provided by the host). Default behaviour is to chain only if the interupt has not been acknowledged.
            // Note that the default handler will always enable interrupts, which makes the no_interrupts option less effective.
            // This option effectively implies no_reentry and no_auto_eoi. 
            always_chain = 0b10,

            // Don't automatically send an End Of Interrupt for this IRQ. The first call to acknowledge() will send the EOI.
            // Default behaviour is to EOI before calling any handlers, allowing interruption by lower-priority IRQs.
            no_auto_eoi = 0b100,

            // Mask the current IRQ while it is being serviced, preventing re-entry.
            no_reentry = 0b1000,

            // Mask all interrupts while this IRQ is being serviced, preventing further interruption.
            no_interrupts = 0b10000
        };
        inline constexpr irq_config_flags operator| (irq_config_flags a, auto b) { return static_cast<irq_config_flags>(static_cast<int>(a) | static_cast<int>(b)); }
        inline constexpr irq_config_flags operator|= (irq_config_flags& a, auto b) { return a = (a | b); }
    }
}

#include <jw/dpmi/detail/irq.h>

namespace jw
{
    namespace dpmi
    {
        // Main IRQ handler class
        class irq_handler : public detail::irq_handler_base, class_lock<irq_handler>
        {
        public:
            using irq_handler_base::irq_handler_base;
            ~irq_handler() { disable(); }

            void set_irq(irq_level i) { disable(); irq = i; }
            void enable() { if (!enabled) detail::irq_controller::get_irq(irq).add(this); enabled = true; }
            void disable() { if (enabled) detail::irq_controller::get_irq(irq).remove(this); enabled = false; }

        private:
            irq_handler(const irq_handler&) = delete;
            irq_handler(irq_handler&&) = delete;
            bool enabled { false };
            irq_level irq { };
        };
    }
}

