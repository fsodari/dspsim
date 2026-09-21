#pragma once
// #include <dspsim/forward.h>
#include <dspsim/model.h>
#include <dspsim/process.h>
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

    /*
        Rather than only allowing Models with an eval function,
        we could register a process.
    */
    class SensitivityEvent
    {
    private:
        Context *_context;
        UniqueStack<Model *> _subscribers;
        UniqueStack<Process *> _processes;

    public:
        SensitivityEvent();
        UniqueStack<Model *> &subscribers();
        UniqueStack<Process *> &processes();
        void add_subscriber(Model *module);
        void add_process(Process *process);

        void notify();
    };
} // namespace dspsim