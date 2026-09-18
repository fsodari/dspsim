#pragma once
#include <dspsim/forward.h>
#include <cstdint>

namespace dspsim
{
    enum EventType
    {
        NoChange,
        Changed,
        Posedge,
        Negedge
    };

    class TimeEvent
    {
    public:
        TimeEvent(Model *subscriber, uint64_t time_update)
            : subscriber(subscriber), time_update(time_update)
        {
        }
        bool operator<(const TimeEvent &other) const { return time_update < other.time_update; }
        bool operator>(const TimeEvent &other) const { return time_update > other.time_update; }

        Model *subscriber;
        uint64_t time_update;
    };
} // namespace dspsim