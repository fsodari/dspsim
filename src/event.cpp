#include <dspsim/event.h>
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
namespace dspsim
{

    SensitivityEvent::SensitivityEvent(Context *context)
        : _context(context)
    {
    }

    UniqueStack<Process *> &SensitivityEvent::processes()
    {
        return _processes;
    }

    void SensitivityEvent::add_process(Process *process)
    {
        _processes.push_back(process);
    }

    void SensitivityEvent::notify()
    {
        for (const auto &p : _processes.stack())
        {
            _context->_process_eval_stack.push_back(p);
        }
    }

    TimeEvent::TimeEvent(Context *context, Process *process, uint64_t time_update)
        : _context(context),
          process(process),
          time_update(time_update)
    {
    }
}
