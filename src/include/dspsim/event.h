#pragma once
#include <dspsim/utils/unique_stack.h>
#include <cstdint>
#include <memory>

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

    Every signal and port owns three of these, and most of them never get a subscriber, so the subscriber
    lists are allocated on first subscription: an unsubscribed event is 24 bytes and notify() on it is a
    single null check. This keeps the per-signal working set of the update phase small.
    */
    class SensitivityEvent
    {

    public:
        struct Subscribers
        {
            // Processes with static sensitivity are notified whenever this event occurs unless they are waiting on a dynamic event.
            UniqueStack<ProcessBase *> static_;
            // Processes with dynamic sensitivity. This list is cleared when the event occurs, each process is removed
            // from its other dynamic events, and it returns to having static sensitivity.
            UniqueStack<ProcessBase *> dynamic_;
        };

        /*
            Events must be linked to a context. Dynamic events will not be able to automatically obtain the context.
        */
        SensitivityEvent(Context *context);
        SensitivityEvent(const SensitivityEvent &) = delete;
        SensitivityEvent &operator=(const SensitivityEvent &) = delete;

        // Notify all subscribed processes that this event has occurred.
        // called after the signal update phase.
        void notify();

        void trigger();

        // Check if the event has occurred in the last evaluation cycle.
        bool has_happened() const;
        operator bool() const { return has_happened(); }

        // Subscribe a process with static sensitivity: it is scheduled whenever the event occurs.
        void subscribe_static(ProcessBase *process);
        // Subscribe a process with dynamic sensitivity: scheduled once, on the next occurrence.
        void subscribe_dynamic(ProcessBase *process);
        void unsubscribe_dynamic(ProcessBase *process);
        // Add the static subscribers of another event (ports hand theirs to the bound signal at elaboration).
        void merge_static_subscribers(const SensitivityEvent &other);

        // True if any process ever subscribed (the lists exist).
        bool has_subscribers() const { return subscribers_ != nullptr; }
        bool has_static_subscribers() const;
        bool has_dynamic_subscribers() const;

        // Direct access to the lists. Allocates them, so prefer the queries above on the hot path.
        UniqueStack<ProcessBase *> &static_subscribers() { return subscribers().static_; }
        UniqueStack<ProcessBase *> &dynamic_subscribers() { return subscribers().dynamic_; }

    private:
        Subscribers &subscribers();

    private:
        Context *context_;
        int64_t change_time_;
        std::unique_ptr<Subscribers> subscribers_;
    };

    /*
        Represents a time-based event that will trigger a process at a specified future time.
    */
    class TimeEvent
    {
    public:
        // wake_count value of an event that always wakes its process (next_trigger(time)).
        static constexpr uint64_t kAnyWake = ~uint64_t{0};

        /*
            Events must be linked to a context. Dynamic events will not be able to automatically obtain the context.
            A coroutine time wait passes the process's wake count: the event is stale, and dropped, if the process
            has been resumed since (see ProcessBase::schedule_time_wait).
        */
        TimeEvent(uint64_t time_update, ProcessBase *process, uint64_t wake_count = kAnyWake);
        uint64_t time_update() const { return time_update_; }
        ProcessBase *process() const { return process_; }
        uint64_t wake_count() const { return wake_count_; }
        // True if the event wakes the process regardless of what resumed it in the meantime.
        bool sticky() const { return wake_count_ == kAnyWake; }

        // Need to expose comparison operators so this can be used with a priority queue or other sorted structure.
        bool operator<(const TimeEvent &other) const { return time_update_ < other.time_update_; }
        bool operator>(const TimeEvent &other) const { return time_update_ > other.time_update_; }

    private:
        uint64_t time_update_;
        ProcessBase *process_;
        uint64_t wake_count_;
    };
} // namespace dspsim
