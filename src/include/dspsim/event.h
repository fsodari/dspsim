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
    class ProcessBase;

    // class Event
    // {
    // private:
    //     Context *_context;

    //     // Processes/coros that are sensitive to this event via their static sensitivity.
    //     // This stack does not get cleared after notifying.
    //     UniqueStack<ProcessBase *> _static_processes;

    //     // Processes/coros that are dynamically sensitive to this event.
    //     // This stack gets cleared after notifying.
    //     UniqueStack<ProcessBase *> _dynamic_processes;

    // public:
    //     Event(Context *context) : _context(context) {}

    //     virtual ~Event() = default;

    //     // Schedule the
    //     virtual void schedule_static(ProcessBase *process) { _static_processes.push_back(process); }
    //     virtual void schedule_dynamic(ProcessBase *process) { _dynamic_processes.push_back(process); }
    //     /*
    //     virtual void schedule(void)
    //     {
    //         // Sensitivity events are created during construction so they exist before scheduling.
    //         context()->_sensitivity_event_stack.push_back(this);

    //         // Time events are created dynamically. How can schedule get called?
    //         // Time event is created inside the awaitable?
    //         context()->_time_event_stack.push_back(this);
    //     }
    //     */
    //     /*
    //     virtual void notify()
    //     {
    //         for (auto &process : _static_processes)
    //         {
    //             context()->_process_eval_stack.push_back(process);
    //         }
    //         for (auto &process : _dynamic_processes)
    //         {
    //             context()->_process_eval_stack.push_back(process);
    //         }
    //         _dynamic_processes.clear();
    //     }
    //     */
    //     virtual void notify() = 0;
    // };

    /*
    Holds lists of processes that are sensitive to this event.
    When notify() is called, all registered processes will be queued for evaluation.
    */
    class SensitivityEvent
    {
    private:
        Context *_context;
        // Source. Processes sensitive this event will need to check if the event has happened.
        // This would need to be resolved during elaboration for coros that are sensitive to a port.
        // SignalBase *_source;

        // Processes with static sensitivity. This is not cleared when the event is notified.
        UniqueStack<ProcessBase *> _static_processes;

        // Processes with dynamic sensitivity. This is cleared when the event is notified.
        UniqueStack<ProcessBase *> _dynamic_processes;

    public:
        /*
            Events must be linked to a context. If dynamic events (TBD) are introduced, then will not be able to automatically obtain the context.
        */
        SensitivityEvent(Context *context);

        // Add a process to this event's list of sensitive processes.
        void add_static_process(ProcessBase *process);
        void add_dynamic_process(ProcessBase *process);

        // Schedule all of this module's processes for evalutation.
        void notify();

        UniqueStack<ProcessBase *> &static_processes();
        UniqueStack<ProcessBase *> &dynamic_processes();
    };

    /*
        Represents a time-based event that will trigger a process at a specified future time.
    */
    class TimeEvent
    {
        Context *_context;

    public:
        ProcessBase *process;
        uint64_t time_update;

    public:
        /*
            Events must be linked to a context. If dynamic events (TBD) are introduced, then will not be able to automatically obtain the context.
        */
        TimeEvent(Context *context, uint64_t time_update, ProcessBase *process);

        // Need to expose comparison operators so this can be used with a priority queue or other sorted structure.
        bool operator<(const TimeEvent &other) const { return time_update < other.time_update; }
        bool operator>(const TimeEvent &other) const { return time_update > other.time_update; }
    };
} // namespace dspsim
