#include <dspsim/coro.h>
#include <dspsim/context.h>
#include <dspsim/process.h>

namespace dspsim
{
    // Task Task::promise_type::get_return_object()
    // {
    //     return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    // }
    // std::suspend_always Task::promise_type::initial_suspend() noexcept
    // {
    //     return {};
    // }

    // std::suspend_always Task::promise_type::final_suspend() noexcept
    // {
    //     return {};
    // }

    // void Task::promise_type::return_void()
    // {
    //     // Clean up context.
    // }

    // void Task::promise_type::unhandled_exception()
    // {
    //     std::terminate();
    // }

    Task::Task(std::coroutine_handle<promise_type> coroutine_handle) noexcept
        : handle(coroutine_handle)
    {
    }

    Task::Task(Task &&other) noexcept
        : handle(std::exchange(other.handle, {}))
    {
    }

    Task &Task::operator=(Task &&other) noexcept
    {
        if (this != &other)
        {
            if (handle)
                handle.destroy();
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }

    Task::~Task()
    {
        if (handle)
        {
            handle.destroy();
        }
    }

    /*
        Time Event awaitable.
    */
    WaitTimeEvent::WaitTimeEvent(uint64_t time_delta, ProcessBase *process)
        : time_delta_(time_delta),
          process_(process)
    {
        process_->context()->schedule_time_delta_event(time_delta_, process_);
    }

    bool WaitTimeEvent::await_ready() const noexcept
    {
        return time_delta_ <= 0; // Always false...
    }

    void WaitTimeEvent::await_suspend(std::coroutine_handle<> h) noexcept
    {
        (void)h;
    }

    void WaitTimeEvent::await_resume() noexcept {}

    /*
        Sensitivity Event awaitable.
    */
    WaitSensitivityEvent::WaitSensitivityEvent(ProcessBase *process)
        : process_(process)
    {
    }

    WaitSensitivityEvent::WaitSensitivityEvent(SensitivityEvent &event, ProcessBase *process)
        : WaitSensitivityEvent(process)
    {
        // events_.push_back(&event);
        dynamic_events_.push_back(&event);
    }

    WaitSensitivityEvent::WaitSensitivityEvent(std::vector<std::reference_wrapper<SensitivityEvent>> events, ProcessBase *process)
        : WaitSensitivityEvent(process)
    {
        for (const auto &event : events)
        {
            dynamic_events_.push_back(&event.get());
            // event.get().dynamic_subscribers().push_back(process_);
        }
    }

    bool WaitSensitivityEvent::await_ready() const noexcept
    {
        // Must wait until the event occurs in the notification phase and the task is explicitly resumed.
        return false;
    }

    void WaitSensitivityEvent::await_suspend(std::coroutine_handle<> h) noexcept
    {
        // Use the handle instead of a process?
        (void)h;
        for (auto &event : dynamic_events_)
        {
            event->dynamic_subscribers().push_back(process_);
        }
    }

    void WaitSensitivityEvent::await_resume() noexcept {}
} // namespace dspsim
