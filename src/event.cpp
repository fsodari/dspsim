#include <dspsim/event.h>
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
namespace dspsim
{
    TimeEvent::TimeEvent(Model *subscriber, uint64_t time_update)
        : subscriber(subscriber), time_update(time_update)
    {
    }

    bool TimeEvent::operator<(const TimeEvent &other) const
    {
        return time_update < other.time_update;
    }

    bool TimeEvent::operator>(const TimeEvent &other) const
    {
        return time_update > other.time_update;
    }

    SensitivityEvent::SensitivityEvent()
        : _context(Context::obtain().get())
    {
    }

    UniqueStack<Model *> &SensitivityEvent::subscribers()
    {
        return _subscribers;
    }
    UniqueStack<Process *> &SensitivityEvent::processes()
    {
        return _processes;
    }

    void SensitivityEvent::add_subscriber(Model *module)
    {
        _subscribers.push_back(module);
    }
    void SensitivityEvent::add_process(Process *process)
    {
        _processes.push_back(process);
    }

    void SensitivityEvent::notify()
    {
        for (auto m : _subscribers)
        {
            _context->_push_eval_stack(m);
        }
        for (auto p : _processes)
        {
            _context->_process_eval_stack.push_back(p);
        }
    }
}
