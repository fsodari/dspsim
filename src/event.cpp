#include <dspsim/event.h>
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <spdlog/spdlog.h>

namespace dspsim
{

    SensitivityEvent::SensitivityEvent(Context *context)
        : context_(context),
          //   id_(context_->next_event_id()),
          static_subscribers_{100}
    {
    }

    void SensitivityEvent::notify()
    {
        if (!static_subscribers_.empty() || !dynamic_subscribers_.empty())
        {
            context_->_sensitivity_event_stack.push_back(this);
        }
        // context_->_sensitivity_event_stack.push_back(this);
    }

    void SensitivityEvent::trigger()
    {
        // for (const auto &process : static_subscribers_.stack())
        // {
        //     context_->_process_eval_stack.push_back(process);
        // }
        // Schedule processes with static sensitivity if they are not currently waiting on a dynamic event.
        for (const auto &process : static_subscribers_)
        {
            if (!process->static_sensitivity_disabled())
            {
                context_->_process_eval_stack.push_back(process);
            }
        }
        // Schedule dynamic processes.

        [[unlikely]] while (!dynamic_subscribers_.empty())
        {
            auto process = dynamic_subscribers_.back();
            dynamic_subscribers_.pop_back();
            context_->_process_eval_stack.push_back(process);
            // Clear the static sensitivity disabled flag for the process, as it has now been notified by a dynamic event.
            process->reset_static_sensitivity();
        }
    }

    TimeEvent::TimeEvent(uint64_t time_update, ProcessBase *process)
        : time_update_(time_update),
          process_(process)
    {
    }
}
