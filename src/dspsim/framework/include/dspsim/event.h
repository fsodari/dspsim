#pragma once
#include <dspsim/utils/unique_stack.h>
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
    class Context;
    class Process;

    /*
        Rather than only allowing Models with an eval function,
        we could register a process.
    */
    class SensitivityEvent
    {
    private:
        Context *_context;
        UniqueStack<Process *> _processes;

    public:
        SensitivityEvent(Context *context);
        UniqueStack<Process *> &processes();
        void add_process(Process *process);

        void notify();
    };

    class TimeEvent
    {
        Context *_context;

    public:
        Process *process;
        uint64_t time_update;

    public:
        TimeEvent(Context *context, Process *process, uint64_t time_update);
        bool operator<(const TimeEvent &other) const;
        bool operator>(const TimeEvent &other) const;
    };
} // namespace dspsim
