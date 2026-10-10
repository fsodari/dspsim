#include <dspsim/coro.h>
#include <dspsim/context.h>
#include <dspsim/process.h>

namespace dspsim
{
    /*
        Time Event awaitable.
    */
    WaitTimeEvent::WaitTimeEvent(uint64_t time_delta, ProcessBase *process)
        : WaitBase(process)
    {
        process->schedule_time_wait(time_delta);
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
        process->schedule_dynamic_event(event);
        schedule_timeout(timeout);
    }

    WaitSensitivityEvent::WaitSensitivityEvent(const std::vector<std::reference_wrapper<SensitivityEvent>> &events, ProcessBase *process, uint64_t timeout)
        : WaitBase(process)
    {
        for (auto &event : events)
        {
            process->schedule_dynamic_event(event.get());
        }
        schedule_timeout(timeout);
    }

    WaitSensitivityEvent::WaitSensitivityEvent(const std::vector<SensitivityEvent *> &events, ProcessBase *process, uint64_t timeout)
        : WaitBase(process)
    {
        for (auto *event : events)
        {
            process->schedule_dynamic_event(*event);
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
        process()->schedule_time_wait(timeout);
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
