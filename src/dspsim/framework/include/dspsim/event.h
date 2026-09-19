#pragma once
#include <dspsim/forward.h>
#include <vector>
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
        Model *subscriber;
        uint64_t time_update;

    public:
        TimeEvent(Model *subscriber, uint64_t time_update);
        bool operator<(const TimeEvent &other) const;
        bool operator>(const TimeEvent &other) const;
    };

    class SensitivityEvent
    {
    private:
        std::vector<Module *> _subscribers;

    public:
        std::vector<Module *> &subscribers();
        void add_subscriber(Module *module);
    };
    // using SensitivityEvent = std::vector<Module *>;
} // namespace dspsim