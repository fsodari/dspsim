#include <dspsim/event.h>
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <spdlog/spdlog.h>

namespace dspsim
{

    SensitivityEvent::SensitivityEvent(Context *context)
        : context_(context),
          id_(context_->next_event_id())
    {
    }

    void SensitivityEvent::notify()
    {
        // Schedule processes with static sensitivity if they are not currently waiting on a dynamic event.
        for (const auto &process : static_subscribers_)
        {
            if (!process->static_sensitivity_disabled())
            {
                context_->_process_eval_stack.push_back(process);
            }
        }
        // Schedule dynamic processes.
        for (const auto &process : dynamic_subscribers_)
        {
            context_->_process_eval_stack.push_back(process);
            // Clear the static sensitivity disabled flag for the process, as it has now been notified by a dynamic event.
            process->reset_static_sensitivity();
        }
        // Clear the dynamic subscribers after notifying.
        dynamic_subscribers_.clear();
    }

    TimeEvent::TimeEvent(uint64_t time_update, ProcessBase *process)
        : time_update_(time_update),
          process_(process)
    {
    }
}
