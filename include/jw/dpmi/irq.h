// Interrupt handling functionality.

#pragma once
#include <function.h>
#include <vector>
#include <atomic>
#include <array>
#include <unordered_map>
#include <map>
#include <deque>
#include <algorithm>
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
//      the handler on the current stack (which should already be locked).
// When a hardware exception occurs and interrupts nest 5 levels deep, it crashes? (exphdlr.c:306)

// HDPMI: does have a "locked" stack (LPMS), not sure why. It doesn't even support virtual memory.
// Also switches to the locked stack only on first interrupt, just like CWSDPMI.
// However, the LPMS use count appears to be a flag instead of a counter...? In that case
// a nested interrupt sequence like INT->INT->IRET->INT would overwrite the first interrupt's stack.
// Yes, such a sequence is very unlikely... but not impossible.
// There is a compile-time flag to toggle counter/flag behaviour (hdpmi.inc:235) but it looks like
// this is not properly implemented (hdpmi.asm:1562).
//      Possible solutions:
//      1. Fix HDPMI
//      2. Don't allow nested interrupts
//      3. Copy the host-provided stack to our own stack and IRET from there.



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


#define INTERRUPT //[[gnu::hot, gnu::flatten, gnu::optimize("O3"), gnu::used]]

namespace jw
{
    namespace dpmi
    {
        using int_vector = std::uint32_t; // easier to use from asm
        using irq_level = std::uint8_t;

        // Configuration flags passed to irq_handler constructor.
        enum irq_config_flags
        {
            // Always call this handler, even if the interrupt has already been acknowledged by a previous handler in the chain.
            always_call = 0b1,

            // Always chain to the default handler (usually provided by the OS). Default behaviour is to chain only if the interupt has not been acknowledged.
            always_chain = 0b10,

            // Don't automatically send an End Of Interrupt for this IRQ. The first call to acknowledge() will send the EOI. 
            // Default behaviour is to EOI before calling any handlers, allowing interruption by lower-priority IRQs.
            no_auto_eoi = 0b100,

            // Mask the current IRQ while it is being serviced, preventing re-entry.
            no_reentry = 0b1000,

            // Mask all interrupts while this IRQ is being serviced, preventing further interruption.
            no_reentry_at_all = 0b10000
        };
        inline constexpr irq_config_flags operator| (irq_config_flags a, auto b) { return static_cast<irq_config_flags>(static_cast<int>(a) | static_cast<int>(b)); }
        inline constexpr irq_config_flags operator|= (irq_config_flags& a, auto b) { return a = (a | b); }

        namespace detail
        {
            class[[gnu::packed]] irq_wrapper
            {
            public:
                using function_ptr = void(*)(int_vector) noexcept;

            private:
                int_vector vec;                 // [eax-0x1C]
                selector ds;                    // [eax-0x18]
                selector es;                    // [eax-0x16]
                selector fs;                    // [eax-0x14]
                selector gs;                    // [eax-0x12]
                function_ptr entry_point;       // [eax-0x10]
                std::array<byte, 0x40> code;    // [eax-0x0C]

            public:
                irq_wrapper(int_vector _vec, function_ptr f) noexcept;
                auto get_ptr(selector cs = get_cs()) const noexcept { return far_ptr32 { cs, reinterpret_cast<std::uintptr_t>(code.data()) }; }
            };

            class irq_handler_base
            {
                friend class irq;
                irq_handler_base(const irq_handler_base& c) = delete;
                irq_handler_base() = delete;

            public:
                template<typename F>
                irq_handler_base(F func, irq_config_flags f) : handler_ptr(std::allocator_arg, locking_allocator<void> { }, std::forward<F>(func)), flags(f) { }
                const func::function<void(void(*)())> handler_ptr;
                const irq_config_flags flags;
            };

            class irq
            {
                std::deque<irq_handler_base*, locking_allocator<>> handler_chain { };
                int_vector vec;
                std::unique_ptr<irq_wrapper> wrapper;
                far_ptr32 old_handler { };
                irq_config_flags flags { };

                void add_flags() { flags = { }; for (auto* p : handler_chain) flags |= p->flags; }

                //DPMI 0.9 AX=0205
                static void set_pm_interrupt_vector(int_vector v, far_ptr32 ptr);

                //DPMI 0.9 AX=0204
                static far_ptr32 get_pm_interrupt_vector(int_vector v);

                irq(const irq&) = delete;
                
                irq(int_vector v, irq_wrapper::function_ptr f) : vec(v), old_handler(get_pm_interrupt_vector(v))
                {
                    wrapper = std::make_unique<irq_wrapper>(v, f);
                    set_pm_interrupt_vector(vec, wrapper->get_ptr());
                    std::cout << "irq handler set up for int " << vec << " at "<< std::hex << (std::uintptr_t)wrapper->get_ptr().offset << std::endl;
                }

                void operator()()
                {
                    for (auto f : handler_chain)
                    {
                        if (f->flags & always_call || !is_acknowledged()) f->handler_ptr(acknowledge);
                    }
                    if (flags & always_chain || !is_acknowledged()) call_far_iret(old_handler);
                }

            public:
                irq(irq&& m) : handler_chain(m.handler_chain), vec(m.vec), wrapper(std::move(m.wrapper)), old_handler(m.old_handler), flags(m.flags)
                {
                    m.old_handler = { };
                }

                ~irq()
                {
                    std::cout << "restoring handler for " << vec << " = "<< old_handler.offset << std::endl;
                    if (old_handler.offset != 0) set_pm_interrupt_vector(vec, old_handler);
                }

                void add(irq_handler_base* p) 
                { 
                    interrupt_mask no_ints_here { };
                    handler_chain.push_back(p); add_flags(); 
                }
                void remove(irq_handler_base* p)
                {
                    interrupt_mask no_ints_here { };
                    std::remove_if(handler_chain.begin(), handler_chain.end(), [p](auto a) { return a == p; });
                    add_flags();
                }

                static irq& get(int_vector v)
                {
                    if (entries.count(v) == 0) entries.emplace(v, irq { v, interrupt_entry_point });
                    return entries.at(v);
                }

                static irq& get_irq(irq_level i) { return get(irq_to_vec(i)); }

            private:
                static int_vector irq_to_vec(irq_level i) 
                { 
                    assert(i < 16); 
                    dpmi::version ver { };
                    return i < 8 ? i + ver.pic_master_base : i + ver.pic_slave_base; 
                }
                static irq_level vec_to_irq(int_vector v) 
                { 
                    dpmi::version ver { };
                    if (v >= ver.pic_master_base && v < ver.pic_master_base + 8u) return v - ver.pic_master_base;
                    if (v >= ver.pic_slave_base && v < ver.pic_slave_base + 8u) return v - ver.pic_slave_base;
                    return 0xff;
                }

                static bool is_irq(int_vector v) { return vec_to_irq(v) != 0xff; }

                static bool is_acknowledged() { return current_int.back() == 0; }

                static void acknowledge()
                {
                    if (is_acknowledged()) return;
                    // TODO: if vec is an IRQ, send EOI to appropriate PIC. count re-entries and EOIs.
                    // don't forget: IRQ2 must ack the second PIC too.
                    // also check PIC's in-service register to prevent double-acknowledge, or acknowledging a spurious irq.
                    if (is_irq(current_int[interrupt_count]))
                    {
                        io::out_port<byte> { 0x20 }.write(0x20); // HACK
                        //outportb(0x20, 0x20);
                    }
                    current_int[interrupt_count] = 0;
                }

                INTERRUPT static byte* get_stack_ptr() { return stacks.at(++interrupt_count).data(); }

                INTERRUPT static void interrupt_entry_point(int_vector vec) noexcept;
                INTERRUPT static void interrupt_call_handler(int_vector vec) noexcept;

                static locked_pool_allocator<> alloc;
                static std::vector<int_vector, locked_pool_allocator<>> current_int; // Current interrupt vector. Set to 0 when acknowlegded.
                //static std::vector<int_vector> current_int;
                static std::unordered_map<int_vector, irq, std::hash<int_vector>, std::equal_to<int_vector>, locking_allocator<>> entries;
                static std::vector<std::array<byte, config::interrupt_stack_size>, locking_allocator<>> stacks;
            };
        }

        // Main IRQ handler class
        class irq_handler : public detail::irq_handler_base, class_lock<irq_handler>
        {
        public:
            using irq_handler_base::irq_handler_base;
            ~irq_handler() { disable(); }

            void set_irq(irq_level i) { disable(); irq = i; }
            void enable() { if (!enabled) detail::irq::get_irq(irq).add(this); enabled = true; }
            void disable() { if (enabled) detail::irq::get_irq(irq).remove(this); enabled = false; }

        private:
            irq_handler(const irq_handler&) = delete;
            bool enabled { false };
            irq_level irq { };
        };
    }
}

