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

    /*
    Holds lists of processes that are sensitive to this event.
    When notify() is called, all registered processes will be queued for evaluation.
    */
    class SensitivityEvent
    {
    private:
        Context *_context;
        uint32_t _id;
        bool *_event_flag = nullptr;

        // Processes with static sensitivity are notified whenever this event occurs unless they are waiting on a dynamic event.
        UniqueStack<ProcessBase *> _static_subscribers;

        // Processes with dynamic sensitivity. This list is cleared when the event occurs and
        // the process will return to having static sensitivity.
        UniqueStack<ProcessBase *> _dynamic_subscribers;

    public:
        /*
            Events must be linked to a context. Dynamic events will not be able to automatically obtain the context.
        */
        SensitivityEvent(Context *context);

        uint32_t id() const { return _id; }

        // Schedule subscribed processes for evaluation.
        // void notify(bool &event_flag);
        void notify();
        void set_event_flag(bool *event_flag) { _event_flag = event_flag; }

        bool has_happened() const { return _event_flag ? *_event_flag : false; }

        UniqueStack<ProcessBase *> &static_subscribers() { return _static_subscribers; }
        UniqueStack<ProcessBase *> &dynamic_subscribers() { return _dynamic_subscribers; }
    };

    /*
        Represents a time-based event that will trigger a process at a specified future time.
    */
    class TimeEvent
    {
        Context *_context;

    public:
        uint64_t time_update;
        ProcessBase *process;

    public:
        /*
            Events must be linked to a context. Dynamic events will not be able to automatically obtain the context.
        */
        TimeEvent(Context *context, uint64_t time_update, ProcessBase *process);

        // Need to expose comparison operators so this can be used with a priority queue or other sorted structure.
        bool operator<(const TimeEvent &other) const { return time_update < other.time_update; }
        bool operator>(const TimeEvent &other) const { return time_update > other.time_update; }
    };
} // namespace dspsim
