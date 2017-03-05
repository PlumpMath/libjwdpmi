#pragma once
#include <atomic>
#include <cstdint>

namespace jw
{
    namespace dpmi
    {
        namespace detail
        {
            extern volatile std::uint32_t interrupt_count;
        }

        inline bool in_interrupt_context() { return detail::interrupt_count > 0; }; // TODO: in_irq_context: use irq::in_service().any()
        inline void throw_if_interrupt() { if (in_interrupt_context()) throw std::runtime_error("called from interrupt"); }; // TODO: specialized exception
    }
}