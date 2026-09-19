#include <dspsim/event.h>
#include <dspsim/model.h>

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

    // SensitivityEvent member function definitions
    std::vector<Module *> &SensitivityEvent::subscribers()
    {
        return _subscribers;
    }
    void SensitivityEvent::add_subscriber(Module *module)
    {
        _subscribers.push_back(module);
    }
} // namespace dspsim