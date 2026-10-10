/*
    Coroutine support: the Task<T> coroutine type and the awaitables that suspend a process.

    Two kinds of awaitable exist:

    - Leaf awaitables (WaitBase subclasses) suspend the *process* on a scheduler event: a time delta, a
      sensitivity event, or either of them (an event with a timeout). They are built by Module::wait() /
      Context::wait(). Awaiting one records the suspended coroutine as the process's resume point, so the
      scheduler resumes the innermost coroutine.
    - Task<T> is a coroutine that can be co_awaited by another coroutine of the same process. The awaiting
      coroutine suspends until the task co_returns, and receives its value (or exception). This is how
      modules offer asynchronous operations: write a coroutine method returning Task<T> that waits on
      leaf awaitables, e.g. AxisRx::receive(n, timeout).

    A Task<> (void) registered with DSPSIM_CORO / Context::register_coro_task is the root of a process;
    Context::run_until(task) runs one to completion from ordinary (non-coroutine) code.
*/
#pragma once
#include <dspsim/process.h>

#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace dspsim
{
    class Context;
    class SensitivityEvent;

    template <typename T = void>
    struct Task;

    namespace detail
    {
        // When a task finishes, continue the coroutine that awaited it (if any).
        struct TaskFinalAwaiter
        {
            bool await_ready() const noexcept { return false; }
            template <typename Promise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> handle) noexcept
            {
                auto continuation = handle.promise().continuation_;
                return continuation ? continuation : std::coroutine_handle<>(std::noop_coroutine());
            }
            void await_resume() const noexcept {}
        };

        struct TaskPromiseBase
        {
            // The coroutine awaiting this task. Empty for a root task (a process body or Python-driven task).
            std::coroutine_handle<> continuation_{};
            std::exception_ptr exception_{};

            // Tasks always start suspended: a process body is resumed by the scheduler, a nested task by its awaiter.
            std::suspend_always initial_suspend() noexcept { return {}; }
            // Suspend at the end so that the result stays readable, and hand control back to the awaiter.
            TaskFinalAwaiter final_suspend() noexcept { return {}; }

            void unhandled_exception()
            {
                if (continuation_)
                {
                    // Delivered to the awaiting coroutine by result().
                    exception_ = std::current_exception();
                }
                else
                {
                    // Rethrowing leaves the coroutine suspended at its final suspend point and propagates the
                    // exception to whoever resumed it (Context::run()/eval()) instead of terminating the program.
                    throw;
                }
            }

            void rethrow_if_failed()
            {
                if (exception_)
                {
                    std::rethrow_exception(std::exchange(exception_, nullptr));
                }
            }
        };

        template <typename T>
        struct TaskPromise : TaskPromiseBase
        {
            std::optional<T> value_{};

            Task<T> get_return_object();
            template <typename U>
                requires std::convertible_to<U &&, T>
            void return_value(U &&value)
            {
                value_.emplace(std::forward<U>(value));
            }
            T result()
            {
                rethrow_if_failed();
                return std::move(*value_);
            }
        };

        template <>
        struct TaskPromise<void> : TaskPromiseBase
        {
            Task<void> get_return_object();
            void return_void() noexcept {}
            void result() { rethrow_if_failed(); }
        };

        // Awaiter returned by co_await on a Task: starts the task, and resumes the awaiter when it completes.
        template <typename T>
        struct TaskAwaiter
        {
            std::coroutine_handle<TaskPromise<T>> handle;

            bool await_ready() const noexcept { return !handle || handle.done(); }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept
            {
                handle.promise().continuation_ = awaiting;
                // Symmetric transfer: run the task until it suspends on a leaf awaitable or completes.
                return handle;
            }
            T await_resume() { return handle.promise().result(); }
        };
    } // namespace detail

    /*
        A coroutine returning T. Owns the coroutine frame.

        - Task<> (void) is the body of a coroutine process: register it with DSPSIM_CORO / register_coro_task.
        - co_await a Task<T> from another coroutine to run it inline and get its value. The task runs in the
          awaiting process: its leaf waits suspend that process.
        - Context::run_until(task) drives a task to completion from non-coroutine code (main, tests).

        A task must be awaited by at most one coroutine, and must not be destroyed while it is suspended
        inside the scheduler (i.e. keep it alive until it is done).
    */
    template <typename T>
    struct Task
    {
        using promise_type = detail::TaskPromise<T>;
        using handle_type = std::coroutine_handle<promise_type>;

        handle_type handle{};

        Task() = default;
        explicit Task(handle_type coroutine_handle) noexcept : handle(coroutine_handle) {}
        Task(const Task &) = delete;
        Task &operator=(const Task &) = delete;
        Task(Task &&other) noexcept : handle(std::exchange(other.handle, {})) {}
        Task &operator=(Task &&other) noexcept
        {
            if (this != &other)
            {
                if (handle)
                {
                    handle.destroy();
                }
                handle = std::exchange(other.handle, {});
            }
            return *this;
        }
        ~Task()
        {
            if (handle)
            {
                handle.destroy();
            }
        }

        // True once the coroutine has run to completion.
        bool done() const noexcept { return !handle || handle.done(); }

        // Give up ownership of the coroutine frame (used when a process takes over the task).
        handle_type release() noexcept { return std::exchange(handle, {}); }

        // The task's value. Only valid once done(); rethrows an exception thrown by the task.
        T result() { return handle.promise().result(); }

        detail::TaskAwaiter<T> operator co_await() noexcept { return {handle}; }
    };

    template <typename T>
    Task<T> detail::TaskPromise<T>::get_return_object()
    {
        return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
    }
    inline Task<void> detail::TaskPromise<void>::get_return_object()
    {
        return Task<void>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
    }

    // Why a wait with a timeout resumed.
    enum class WaitResult
    {
        // One of the awaited events triggered.
        Triggered,
        // The timeout elapsed before any event triggered.
        Timeout
    };

    /*
        Base class of the leaf awaitables. A leaf awaitable schedules its process to be resumed by the
        scheduler and suspends the coroutine awaiting it. Subclasses schedule the wait in their
        constructor (so construct one only to await it) and may return a value from await_resume().

        To add a new leaf awaitable: derive from WaitBase, schedule the wakeup in the constructor through
        ProcessBase (schedule_dynamic_event / schedule_time_wait), and keep await_suspend() as is.
    */
    class WaitBase
    {
    public:
        explicit WaitBase(ProcessBase *process) noexcept : process_(process) {}

        // The wait is always scheduled on construction, so the coroutine must suspend.
        bool await_ready() const noexcept { return false; }
        // Record where the process resumes: the innermost suspended coroutine.
        void await_suspend(std::coroutine_handle<> handle) noexcept { process_->set_resume_point(handle); }

        ProcessBase *process() const noexcept { return process_; }

        // Python's __await__ must call once to suspend, then again to complete. This mirrors a single suspend point.
        bool py_step_done() noexcept
        {
            bool done = suspended_;
            suspended_ = true;
            return done;
        }

    private:
        ProcessBase *process_;
        bool suspended_ = false;
    };

    // Wait on the process's static sensitivity list.
    class Wait : public WaitBase
    {
    public:
        explicit Wait(ProcessBase *process) noexcept : WaitBase(process) {}
        void await_resume() const noexcept {}
    };

    /*
        Awaitable for waiting a time delta. The constructor schedules the wait. The process ignores its static
        sensitivity until the time has elapsed.
    */
    class WaitTimeEvent : public WaitBase
    {
    public:
        WaitTimeEvent(uint64_t time_delta, ProcessBase *process);
        void await_resume() const noexcept {}
    };

    /*
        Awaitable for sensitivity events, optionally with a timeout. The constructor schedules the wait:
        the process subscribes to the events immediately, so construct it only to await it.
        Waiting on several events resumes on whichever triggers first, and the process is then unsubscribed
        from the others. With a timeout, the process also resumes once the timeout elapses, unsubscribed from
        all of the events; await_resume() tells which happened. A timeout of 0 means no timeout.
    */
    class WaitSensitivityEvent : public WaitBase
    {
    public:
        // Wait on any event in the static sensitivity list.
        explicit WaitSensitivityEvent(ProcessBase *process);

        // Wait on a dynamically added event.
        WaitSensitivityEvent(SensitivityEvent &event, ProcessBase *process, uint64_t timeout = 0);
        WaitSensitivityEvent(std::reference_wrapper<SensitivityEvent> event, ProcessBase *process, uint64_t timeout = 0)
            : WaitSensitivityEvent(event.get(), process, timeout) {}

        // Wait on any of several dynamic events.
        WaitSensitivityEvent(const std::vector<std::reference_wrapper<SensitivityEvent>> &events, ProcessBase *process, uint64_t timeout = 0);
        // Wait on any of several dynamic events, given by pointer (used by the Python bindings).
        WaitSensitivityEvent(const std::vector<SensitivityEvent *> &events, ProcessBase *process, uint64_t timeout = 0);

        // WaitResult::Timeout if the timeout elapsed before an event triggered, else WaitResult::Triggered.
        WaitResult await_resume() const noexcept;

    private:
        void schedule_timeout(uint64_t timeout);

    private:
        bool has_timeout_ = false;
        uint64_t deadline_ = 0;
    };
} // namespace dspsim
