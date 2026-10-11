#include <dspsim/coro.h>
#include <dspsim/context.h>
#include <dspsim/process.h>

namespace dspsim
{
    void WaitBase::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        process_->set_resume_point(handle);
    }

    bool WaitBase::py_step_done() noexcept
    {
        bool done = suspended_;
        suspended_ = true;
        return done;
    }

    void WaitBase::schedule_event(SensitivityEvent &event)
    {
        process_->schedule_dynamic_event(event);
    }

    void WaitBase::schedule_time(uint64_t time_delta)
    {
        process_->schedule_time_wait(time_delta);
    }

    /*
        Time Event awaitable.
    */
    WaitTimeEvent::WaitTimeEvent(uint64_t time_delta, ProcessBase *process)
        : WaitBase(process)
    {
        schedule_time(time_delta);
    }

    /*
        Sensitivity Event awaitable.
    */
    WaitSensitivityEvent::WaitSensitivityEvent(ProcessBase *process)
        : WaitBase(process)
    {
        // Static sensitivity is already registered. Nothing to schedule.
    }

    WaitSensitivityEvent::WaitSensitivityEvent(SensitivityEvent &event, ProcessBase *process, uint64_t timeout)
        : WaitBase(process)
    {
        schedule_event(event);
        schedule_timeout(timeout);
    }

    WaitSensitivityEvent::WaitSensitivityEvent(const std::vector<std::reference_wrapper<SensitivityEvent>> &events, ProcessBase *process, uint64_t timeout)
        : WaitBase(process)
    {
        for (auto &event : events)
        {
            schedule_event(event.get());
        }
        schedule_timeout(timeout);
    }

    WaitSensitivityEvent::WaitSensitivityEvent(const std::vector<SensitivityEvent *> &events, ProcessBase *process, uint64_t timeout)
        : WaitBase(process)
    {
        for (auto *event : events)
        {
            schedule_event(*event);
        }
        schedule_timeout(timeout);
    }

    void WaitSensitivityEvent::schedule_timeout(uint64_t timeout)
    {
        if (timeout == 0)
        {
            return;
        }
        has_timeout_ = true;
        deadline_ = process()->context()->time() + timeout;
        schedule_time(timeout);
    }

    WaitResult WaitSensitivityEvent::await_resume() const noexcept
    {
        // The timeout is processed before any event of the same time step, and cancels the events, so the
        // process reaches the deadline only through the timeout.
        if (has_timeout_ && process()->context()->time() >= deadline_)
        {
            return WaitResult::Timeout;
        }
        return WaitResult::Triggered;
    }
} // namespace dspsim
