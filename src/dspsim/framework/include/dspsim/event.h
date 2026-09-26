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
        Holds a static list of processes that are sensitive to this event.
        When notify() is called, all registered processes will be queued for evaluation.
    */
    class SensitivityEvent
    {
    private:
        Context *_context;
        UniqueStack<Process *> _processes;

    public:
        /*
            Events must be linked to a context. If dynamic events (TBD) are introduced, then will not be able to automatically obtain the context.
        */
        SensitivityEvent(Context *context);

        // Add a process to this event's list of sensitive processes.
        void add_process(Process *process);

        // Schedule all of this module's processes for evalutation.
        void notify();

        UniqueStack<Process *> &processes();
    };

    /*
        Represents a time-based event that will trigger a process at a specified future time.
    */
    class TimeEvent
    {
        Context *_context;

    public:
        Process *process;
        uint64_t time_update;

    public:
        /*
            Events must be linked to a context. If dynamic events (TBD) are introduced, then will not be able to automatically obtain the context.
        */
        TimeEvent(Context *context, Process *process, uint64_t time_update);

        // Need to expose comparison operators so this can be used with a priority queue or other sorted structure.
        bool operator<(const TimeEvent &other) const { return time_update < other.time_update; }
        bool operator>(const TimeEvent &other) const { return time_update > other.time_update; }
    };
} // namespace dspsim
