#include <dspsim/coro/coro.h>
#include <dspsim/context.h>
#include <dspsim/process.h>

namespace dspsim
{
    Task Task::promise_type::get_return_object()
    {
        return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always Task::promise_type::initial_suspend() noexcept
    {
        return {};
    }

    std::suspend_always Task::promise_type::final_suspend() noexcept
    {
        return {};
    }

    void Task::promise_type::return_void()
    {
        // Clean up context.
    }

    void Task::promise_type::unhandled_exception()
    {
        std::terminate();
    }

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
    WaitTimeEvent::WaitTimeEvent(uint64_t time_delta, Context *context, ProcessBase *process)
        : _time_delta(time_delta), _context(context), _process(process)
    {
    }

    bool WaitTimeEvent::await_ready() const noexcept
    {
        return _time_delta <= 0; // Always false...
    }

    void WaitTimeEvent::await_suspend(std::coroutine_handle<> h) noexcept
    {
        (void)h;
        _context->schedule_time_delta_event(_time_delta, _process);
    }

    void WaitTimeEvent::resume() {}

    void WaitTimeEvent::await_resume() noexcept {}

    /*
        Sensitivity Event awaitable.
    */
    WaitSensitivityEvent::WaitSensitivityEvent(Context *context, ProcessBase *process)
        : _context(context), _process(process)
    {
        if (_process == nullptr)
        {
            _process = _context->_current_process;
        }
    }

    WaitSensitivityEvent::WaitSensitivityEvent(SensitivityEvent *event, Context *context, ProcessBase *process)
        : WaitSensitivityEvent(context, process)
    {
        _events.push_back(event);
    }

    WaitSensitivityEvent::WaitSensitivityEvent(std::initializer_list<SensitivityEvent *> events, Context *context, ProcessBase *process)
        : WaitSensitivityEvent(context, process)
    {
        _events.push_range(events);
    }

    bool WaitSensitivityEvent::await_ready() const noexcept
    {
        // return _event->has_happened();

        // Must wait until the event occurs in the notification phase and the task is explicitly resumed.
        return false;
    }

    void WaitSensitivityEvent::await_suspend(std::coroutine_handle<> h) noexcept
    {
        (void)h;
        for (auto &event : _events)
        {
            event->dynamic_subscribers().push_back(_process);
        }
    }

    void WaitSensitivityEvent::resume() {}

    void WaitSensitivityEvent::await_resume() noexcept {}
} // namespace dspsim
