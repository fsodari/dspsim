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

    wait::wait(uint64_t time_delta, Context *context, ProcessBase *process)
        : _time_delta(time_delta), _context(context), _process(process)
    {
    }

    bool wait::await_ready() const noexcept
    {
        return _time_delta <= 0;
    }

    void wait::await_suspend(std::coroutine_handle<> h) noexcept
    {
        (void)h;
        _context->schedule_time_delta_event(_time_delta, _process);
    }

    void wait::resume() {}

    void wait::await_resume() noexcept {}

} // namespace dspsim
