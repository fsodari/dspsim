#include <dspsim/event.h>
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <spdlog/spdlog.h>

namespace dspsim
{

    SensitivityEvent::SensitivityEvent(Context *context)
        : _context(context)
    {
    }

    void SensitivityEvent::add_static_process(ProcessBase *process)
    {
        _static_processes.push_back(process);
    }

    void SensitivityEvent::add_dynamic_process(ProcessBase *process)
    {
        _dynamic_processes.push_back(process);
    }

    void SensitivityEvent::notify()
    {
        for (const auto &p : _static_processes.stack())
        {
            _context->_process_eval_stack.push_back(p);
        }
        for (const auto &p : _dynamic_processes.stack())
        {
            _context->logger->debug("Notifying dynamic process: {}", p->name());
            _context->_process_eval_stack.push_back(p);
        }
        _dynamic_processes.clear();
    }

    UniqueStack<ProcessBase *> &SensitivityEvent::static_processes()
    {
        return _static_processes;
    }

    UniqueStack<ProcessBase *> &SensitivityEvent::dynamic_processes()
    {
        return _dynamic_processes;
    }

    TimeEvent::TimeEvent(Context *context, uint64_t time_update, ProcessBase *process)
        : _context(context),
          process(process),
          time_update(time_update)
    {
    }
}
