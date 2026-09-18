#pragma once
#include <cstdint>

namespace dspsim
{
    class Model;
    class TimeEvent
    {
    public:
        TimeEvent(Model *subscriber, uint64_t time_update);
        bool operator<(const TimeEvent &other) const;

        Model *subscriber;
        uint64_t time_update;
    };
} // namespace dspsim