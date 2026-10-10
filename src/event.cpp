#include <dspsim/event.h>
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <spdlog/spdlog.h>

namespace dspsim
{

    SensitivityEvent::SensitivityEvent(Context *context)
        : context_(context),
          change_time_{-1},
          subscribers_(nullptr)
    {
    }

    SensitivityEvent::Subscribers &SensitivityEvent::subscribers()
    {
        if (!subscribers_)
        {
            subscribers_ = std::make_unique<Subscribers>();
        }
        return *subscribers_;
    }

    void SensitivityEvent::notify()
    {
        // Most events never have a subscriber: nothing to schedule, and the event does not "happen".
        if (!subscribers_)
        {
            return;
        }
        if (!subscribers_->static_.empty() || !subscribers_->dynamic_.empty())
        {
            context_->_sensitivity_event_stack.push_back(this);
            change_time_ = context_->update_count();
        }
    }

    void SensitivityEvent::trigger()
    {
        // Only notified events are triggered, and those have subscriber lists.
        Subscribers &subs = *subscribers_;
        // Schedule processes with static sensitivity if they are not currently waiting on a dynamic event.
        for (const auto &process : subs.static_)
        {
            if (!process->static_sensitivity_disabled())
            {
                context_->_process_eval_stack.push_back(process);
            }
        }
        // Schedule dynamic processes.

        [[unlikely]] while (!subs.dynamic_.empty())
        {
            auto process = subs.dynamic_.back();
            subs.dynamic_.pop_back();
            context_->_process_eval_stack.push_back(process);
            // Unsubscribe from the process's other dynamic events and restore its static sensitivity.
            process->dynamic_event_triggered(*this);
        }
    }

    bool SensitivityEvent::has_happened() const
    {
        return change_time_ == context_->update_count();
    }

    void SensitivityEvent::subscribe_static(ProcessBase *process)
    {
        subscribers().static_.push_back(process);
    }

    void SensitivityEvent::subscribe_dynamic(ProcessBase *process)
    {
        subscribers().dynamic_.push_back(process);
    }

    void SensitivityEvent::unsubscribe_dynamic(ProcessBase *process)
    {
        if (subscribers_)
        {
            subscribers_->dynamic_.erase(process);
        }
    }

    void SensitivityEvent::merge_static_subscribers(const SensitivityEvent &other)
    {
        if (other.subscribers_ && !other.subscribers_->static_.empty())
        {
            subscribers().static_.push_range(other.subscribers_->static_);
        }
    }

    bool SensitivityEvent::has_static_subscribers() const
    {
        return subscribers_ && !subscribers_->static_.empty();
    }

    bool SensitivityEvent::has_dynamic_subscribers() const
    {
        return subscribers_ && !subscribers_->dynamic_.empty();
    }

    TimeEvent::TimeEvent(uint64_t time_update, ProcessBase *process, uint64_t wake_count)
        : time_update_(time_update),
          process_(process),
          wake_count_(wake_count)
    {
    }
}
