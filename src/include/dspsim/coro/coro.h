#pragma once
#include <iostream>
#include <coroutine>
#include <queue>
#include <string>
#include <utility>

namespace dspsim
{
    class Context;
    class ProcessBase;

    struct Task
    {
        struct promise_type
        {
            /*
            When a coroutine function is first invoked, the compiler sets up the coroutine state,
            creates the promise_type object, and immediately calls promise.get_return_object().
            The object returned by this function is what the caller of the coroutine function
            actually receives when the coroutine first suspends or finishes
            */
            Task get_return_object();
            // Task get_return_object() { return {}; }
            // returning suspend_always here means the coroutine will always suspend initially.
            // returning suspend_never would start immediately.
            std::suspend_always initial_suspend() noexcept;

            // Called when the coroutine exits. suspend_always will suspend the coroutine at the end, requiring cleanup.
            // suspend_never will immediately destroy the coroutine without suspending at the end.
            std::suspend_always final_suspend() noexcept;

            // Executed on co_return. Other option is return_value(T) if the coroutine returns a value.
            // This needs to clean up the context and remove any reference to the coroutine process so that it
            // doesn't get triggered again.
            void return_void();
            void unhandled_exception();
        };

        //
        std::coroutine_handle<promise_type> handle{};

        Task() = default;
        explicit Task(std::coroutine_handle<promise_type> coroutine_handle) noexcept;
        Task(const Task &) = delete;
        Task &operator=(const Task &) = delete;

        Task(Task &&other) noexcept;

        Task &operator=(Task &&other) noexcept;

        ~Task();
    };

    // Awaitable for waiting a specific time delta
    class wait
    {
        uint64_t _time_delta;
        Context *_context;
        ProcessBase *_process;
        // Event *_event;
        // Coro
    public:
        wait(uint64_t time_delta, Context *context, ProcessBase *process = nullptr);

        // Determine if the coroutine needs to suspend or it can continue immediately.
        // relative events will never be ready immediately. Absolute events could. Or passing 0 would do nothing?
        bool await_ready() const noexcept;

        // If await_ready is false, Schedule the time event and coro is suspended.
        void await_suspend(std::coroutine_handle<> h) noexcept;

        // Scheduler will call this to resume execution.
        void resume();

        // Returns result when the coroutine resumes. Can return a value if needed.
        void await_resume() noexcept;
    };

    /*
    class SomeModule : public Module
    {
        ProcessBase *_coro_process;

    public:
        SomeModule(ModuleName name)
            : Module(name)
        {
            context()->_processes.emplace_back(std::make_unique<CoroProcess>(std::move(some_task()), "some_task"));
            _coro_process = context()->_processes.back().get();
        }

        Task some_task()
        {
            while (true)
            {
                co_await wait{10, this->context(), _coro_process};
            }
        }
    };
    */
} // namespace dspsim
