#pragma once
#include <dspsim/utils/unique_stack.h>

#include <coroutine>
#include <string>
#include <utility>
#include <vector>
#include <initializer_list>

namespace dspsim
{
    class Context;
    class ProcessBase;
    class SensitivityEvent;

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
    class WaitTimeEvent
    {

    public:
        WaitTimeEvent(uint64_t time_delta, ProcessBase *process);

        // Determine if the coroutine needs to suspend or it can continue immediately.
        // relative events will never be ready immediately. Absolute events could. Or passing 0 would do nothing?
        bool await_ready() const noexcept;

        // If await_ready is false, the coroutine is suspended and await_suspend is called.
        // The handle is the coroutine handle that is being suspended.
        void await_suspend(std::coroutine_handle<> handle) noexcept;

        // Called when the coroutine resumes. Can return a value if needed.
        void await_resume() noexcept;

    private:
        uint64_t time_delta_;
        ProcessBase *process_;
    };

    class Wait
    {
    public:
        Wait() {}
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept { (void)handle; }
        void await_resume() noexcept {}
    };

    class WaitSensitivityEvent
    {

    public:
        // Wait on any event in the static sensitivity list.
        WaitSensitivityEvent(ProcessBase *process);

        // Wait on a dynamically added event.
        WaitSensitivityEvent(SensitivityEvent &event, ProcessBase *process);

        // Wait on multiple dynamic events, if any occur (or list);
        WaitSensitivityEvent(std::initializer_list<std::reference_wrapper<SensitivityEvent>> events, ProcessBase *process);

        bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> h) noexcept;
        void await_resume() noexcept;

    private:
        // 0 initial capacity: this is constructed fresh on every co_await in hot loops, and the
        // common case (waiting on already-registered static sensitivity) never pushes any events,
        // so an eager reserve() here would mean two heap allocations per resume for nothing.
        UniqueStack<SensitivityEvent *> events_{0};
        ProcessBase *process_;
    };
} // namespace dspsim
