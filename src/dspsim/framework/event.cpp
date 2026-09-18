#include <dspsim/event.h>
#include <dspsim/model.h>

namespace dspsim
{
    TimeEvent::TimeEvent(Model *subscriber, uint64_t time_update)
        : _subscriber(subscriber), _time_update(time_update)
    {
    }

    bool TimeEvent::operator<(const TimeEvent &other) const
    {
        return _time_update < other._time_update;
    }

    Model *TimeEvent::model() const
    {
        return _subscriber;
    }

    uint64_t TimeEvent::time_update() const
    {
        return _time_update;
    }
} // namespace dspsim