#pragma once
// #include <dspsim/forward.h>
#include <dspsim/model.h>
// #include <dspsim/module.h>
#include <dspsim/utils/unique_stack.h>
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
        // std::vector<Module *> _subscribers;
        UniqueStack<Model *> _subscribers;

    public:
        UniqueStack<Model *> &subscribers();
        void add_subscriber(Model *module);
    };
    // using SensitivityEvent = std::vector<Module *>;
} // namespace dspsim