#include <dspsim/event.h>
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <spdlog/spdlog.h>

namespace dspsim
{

    SensitivityEvent::SensitivityEvent(Context *context)
        : _context(context), _id(_context->next_event_id())
    {
    }

    void SensitivityEvent::notify()
    {
        // Schedule processes with static sensitivity if they are not currently waiting on a dynamic event.
        for (const auto &process : _static_subscribers)
        {
            if (!process->static_sensitivity_disabled())
            {
                _context->_process_eval_stack.push_back(process);
            }
        }
        // Schedule dynamic processes.
        for (const auto &process : _dynamic_subscribers)
        {
            _context->_process_eval_stack.push_back(process);
            // Clear the static sensitivity disabled flag for the process, as it has now been notified by a dynamic event.
            process->reset_static_sensitivity();
        }
        // Clear the dynamic subscribers after notifying.
        _dynamic_subscribers.clear();
    }

    TimeEvent::TimeEvent(Context *context, uint64_t time_update, ProcessBase *process)
        : _context(context),
          time_update(time_update),
          process(process)
    {
    }
}
