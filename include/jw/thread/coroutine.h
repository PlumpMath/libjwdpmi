#pragma once
#include <jw/thread/task.h>

namespace jw
{
    namespace thread
    {
        namespace detail
        {
            template<typename sig, std::size_t stack_bytes = default_stack_size>
            class coroutine_impl;

            template<typename R, typename... A, std::size_t stack_bytes>
            class coroutine_impl<R(A...), stack_bytes> : public task_base<stack_bytes>
            {
                template<typename, std::size_t> friend class coroutine;
                using function_sig = void(coroutine_impl<R(A...), stack_bytes>&, A...);
                using base = task_base<stack_bytes>;

                std::function<function_sig> function;
                std::unique_ptr<std::tuple<A...>> arguments;
                std::unique_ptr<R> result;

            protected:
                virtual void call() override { call(std::index_sequence_for<A...>()); }
                template <std::size_t... i> void call(std::index_sequence<i...>) { function(*this, std::get<i>(std::move(*arguments))...); }

            public:
                // Start the coroutine thread using the specified arguments.
                constexpr void start(A... args)
                {
                    if (this->is_running()) return; // or throw...?
                    arguments = std::make_unique<std::tuple<A...>>(std::forward<A>(args)...);
                    result.reset();
                    base::start();
                }

                // Blocks until the coroutine yields a result, or terminates.
                // Returns true when it is safe to call await() to obtain the result.
                // May rethrow unhandled exceptions!
                bool try_await()
                {
                    dpmi::throw_if_irq();
                    if (scheduler::is_current_thread(this)) return false;

                    this->try_await_while([this]() { return this->state == running; });

                    if (this->state != suspended) return false;
                    return true;
                }

                // Awaits a result from the coroutine.
                // Throws illegal_await if the coroutine ends without yielding a result.
                // May rethrow unhandled exceptions!
                auto await()
                {
                    if (!try_await()) throw illegal_await(this->shared_from_this());

                    this->state = running;
                    return std::move(*result);
                }

                // Called by the coroutine thread to yield a result.
                // This suspends the coroutine until the result is obtained by calling await().
                void yield(auto value)
                {
                    if (!scheduler::is_current_thread(this)) return; // or throw?

                    result = std::make_unique<R>(std::move(value));
                    this->state = suspended;
                    yield();
                    result.reset();
                }

                coroutine_impl(std::function<function_sig> f) : function(f) { }
            };
        }

        template<typename sig, std::size_t stack_bytes = default_stack_size>
        class coroutine;

        template<typename R, typename... A, std::size_t stack_bytes>
        class coroutine<R(A...), stack_bytes>
        {
            using task_type = coroutine_impl<R(A...), stack_bytes>;
            std::shared_ptr<task_type> ptr;

        public:
            constexpr const auto get_ptr() const { return ptr; }
            constexpr auto* operator->() const { return ptr.get(); }
            constexpr auto& operator*() const { return *ptr; }

            template<typename F>
            constexpr coroutine(F f) : ptr(std::make_shared<task_type>(f)) { }

            constexpr coroutine(const coroutine&) = default;
            constexpr coroutine() = delete;
        };
    }
}
