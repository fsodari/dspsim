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

    public:
        /*
            Events must be linked to a context. Dynamic events will not be able to automatically obtain the context.
        */
        SensitivityEvent(Context *context);

        // Notify all subscribed processes that this event has occurred.
        // called after the signal update phase.
        void notify();

        void trigger();

        // Check if the event has occurred in the last evaluation cycle.
        bool has_happened() const;
        operator bool() const { return has_happened(); }

        // Need to encapsulate this better.
        UniqueStack<ProcessBase *> &static_subscribers() { return static_subscribers_; }
        UniqueStack<ProcessBase *> &dynamic_subscribers() { return dynamic_subscribers_; }

    private:
        Context *context_;

        // Processes with static sensitivity are notified whenever this event occurs unless they are waiting on a dynamic event.
        UniqueStack<ProcessBase *> static_subscribers_;

        // Processes with dynamic sensitivity. This list is cleared when the event occurs and
        // the process will return to having static sensitivity.
        UniqueStack<ProcessBase *> dynamic_subscribers_;
        int64_t change_time_;
    };

    /*
        Represents a time-based event that will trigger a process at a specified future time.
    */
    class TimeEvent
    {
    public:
        /*
            Events must be linked to a context. Dynamic events will not be able to automatically obtain the context.
        */
        TimeEvent(uint64_t time_update, ProcessBase *process);
        uint64_t time_update() const { return time_update_; }
        ProcessBase *process() const { return process_; }

        // Need to expose comparison operators so this can be used with a priority queue or other sorted structure.
        bool operator<(const TimeEvent &other) const { return time_update_ < other.time_update_; }
        bool operator>(const TimeEvent &other) const { return time_update_ > other.time_update_; }

    private:
        uint64_t time_update_;
        ProcessBase *process_;
    };
} // namespace dspsim
