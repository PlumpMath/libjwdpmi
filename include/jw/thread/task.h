#pragma once
#include <jw/thread/detail/thread.h>
#include <jw/thread/detail/scheduler.h>
#include <jw/thread/thread.h>

namespace jw
{
    namespace thread
    {
        namespace detail
        {
            template<std::size_t stack_bytes>
            class task_base : public detail::thread, public std::enable_shared_from_this<task_base<stack_bytes>>
            {
                alignas(0x10) std::array<byte, stack_bytes> stack;

            protected:
                constexpr task_base() : thread(stack_bytes, stack.data()) { }

                constexpr void start()
                {
                    if (this->is_running()) return;

                    this->state = starting;
                    this->parent = scheduler::current_thread;
                    scheduler::thread_switch(this->shared_from_this());
                }

            public:
                // Aborts the task prematurely.
                // This throws an abort_thread exception on the thread, allowing the task to clean up and return normally.
                void abort(bool wait = true)
                {
                    if (!this->is_running()) return;

                    this->state = terminating;
                    if (wait && !scheduler::is_current_thread(this))
                        yield_while([&]() { return this->is_running(); });
                }
            };

            template<typename sig, std::size_t stack_bytes = default_stack_size>
            class task_impl;

            template<typename R, typename... A, std::size_t stack_bytes>
            class task_impl<R(A...), stack_bytes> : public task_base<stack_bytes>
            {
                template<typename, std::size_t> friend class task;
                using base = task_base<stack_bytes>;

            protected:
                std::function<R(A...)> function;
                std::unique_ptr<std::tuple<A...>> arguments;
                std::unique_ptr<typename std::conditional<std::is_void<R>::value, int, R>::type> result; // std::experimental::optional ?

                // std::experimental::apply ?
                virtual void call() override { call(std::is_void<R>(), std::index_sequence_for<A...>()); }                                          // Determine if R is void
                template <std::size_t... i> void call(std::true_type, std::index_sequence<i...> seq) { call(seq); }                                 // Void, discard non-existent result.
                template <std::size_t... i> void call(std::false_type, std::index_sequence<i...> seq) { result = std::make_unique<R>(call(seq)); }  // Not void, save result.
                template <std::size_t... i> auto call(std::index_sequence<i...>) { return function(std::get<i>(std::move(*arguments))...); }

                auto get_result(std::true_type) { }
                auto get_result(std::false_type) { return std::move(*result); }

            public:
                // Start the task using the specified arguments.
                constexpr void start(A... args)
                {
                    if (this->is_running()) return;
                    arguments = std::make_unique<std::tuple<A...>>(std::forward<A>(args)...);
                    result.reset();
                    base::start();
                }

                // Blocks until the task returns a result, or terminates.
                // Returns true when it is safe to call await() to obtain the result.
                // TODO: may throw unhandled exceptions
                bool try_await()
                {
                    if (scheduler::is_current_thread(this)) return false;

                    scheduler::get_current_thread()->awaiting = this->shared_from_this();
                    yield_while([&]() { return this->is_running(); });
                    scheduler::get_current_thread()->awaiting.reset();

                    if (this->state == initialized) return false;
                    return true;
                }

                // Awaits a result from the task.
                // Throws illegal_await if the task terminates without returning a result.
                auto await()
                {
                    if (!try_await()) throw illegal_await(this->shared_from_this());

                    this->state = initialized;
                    return get_result(std::is_void<R>());
                }

                constexpr void suspend() noexcept { if (this->state == running) this->state = suspended; }
                constexpr void resume() noexcept { if (this->state == suspended) this->state = running; }

                task_impl(std::function<R(A...)> f) : function(f) { this->allow_orphan = std::is_void<R>::value; }
            };
        }

        template<typename sig, std::size_t stack_bytes = default_stack_size>
        class task;

        template<typename R, typename... A, std::size_t stack_bytes>
        class task<R(A...), stack_bytes>
        {
            using task_type = detail::task_impl<R(A...), stack_bytes>;
            detail::thread_ptr ptr;

        public:
            constexpr const auto get_ptr() const noexcept { return ptr; }
            constexpr auto* operator->() const noexcept { return ptr.get(); }
            constexpr auto& operator*() const noexcept { return *ptr; }

            template<typename F>
            constexpr task(F f) : ptr(std::make_shared<task_type>(f)) { }
            
            template<typename F, typename A>
            constexpr task(std::allocator_arg_t, A alloc, F f) : ptr(std::allocate_shared<task_type>(alloc, f)) { }

            constexpr task(const task&) = default;
            constexpr task() = delete;
        };
    }
}
