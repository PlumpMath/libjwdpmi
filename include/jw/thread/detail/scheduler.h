#pragma once
#include <iostream>
#include <functional>
#include <memory>
#include <deque> 
#include <jw/thread/detail/thread.h>
#include <jw/dpmi/irq_check.h>

// TODO: only rethrow exceptions when a task is being awaited!
// TODO: task->delayed_start(), to schedule a task without immediately starting it.
// TODO: make threads use custom allocator. deque threads should use locked_pool_allocator.
// TODO: use threads.push_front() instead of next_thread.
// TODO: don't save flags!
// TODO: make errno thread-local? other globals?
// TODO: name threads, to aid debugging

namespace jw
{
    namespace thread
    {
        void yield();

        namespace detail
        {
            class scheduler
            {
                template<std::size_t> friend class task_base;
                friend void ::jw::thread::yield();
                static std::deque<thread_ptr> threads;
                static thread_ptr current_thread;
                static thread_ptr main_thread;

            public:
                static bool is_current_thread(const thread* t) noexcept { return current_thread.get() == t; }
                static std::weak_ptr<thread> get_current_thread() noexcept { return current_thread; }

            private:
                [[gnu::noinline, gnu::noclone, gnu::no_stack_limit, gnu::hot]] static void context_switch() noexcept;
                [[gnu::hot]] static void thread_switch(thread_ptr = nullptr);
                [[gnu::noinline, hot]] static void set_next_thread() noexcept;
                static bool is_thread_exception(const std::exception&) noexcept;
                static void check_exception();
                static void catch_thread_exception() noexcept;

                [[gnu::used]] static void run_thread() noexcept;

                class init_main { public: init_main(); } static initializer;
            };
        }
    }
}