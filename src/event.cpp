#include <dspsim/event.h>
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
}