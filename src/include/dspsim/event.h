#pragma once
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
    class PortBase;

    namespace testing
    {
        struct EngineInspector;
    }

    /*
    Holds lists of processes that are sensitive to this event.
    When the event is notified, all registered processes will be queued for evaluation.

    Every signal and port owns three of these, and most of them never get a subscriber, so the subscriber
    lists are allocated on first subscription: an unsubscribed event is 24 bytes and a signal that notifies
    it does a single null check. This keeps the per-signal working set of the update phase small.
    */
    class SensitivityEvent
    {
        // The scheduler triggers notified events, and processes and ports manage their subscriptions.
        friend class Context;
        friend class ProcessBase;
        friend class PortBase;
        // A signal notifies its events in the update phase (notify_committed()).
        template <typename T>
        friend class Signal;
        friend struct testing::EngineInspector;

    public:
        /*
            Events must be linked to a context. Dynamic events will not be able to automatically obtain the context.
        */
        SensitivityEvent(Context *context);
        SensitivityEvent(const SensitivityEvent &) = delete;
        SensitivityEvent &operator=(const SensitivityEvent &) = delete;
        ~SensitivityEvent();

        /*
            Raise the event, from a process or from outside the simulation between two runs. The event occurs
            when the current evaluation phase is over: the processes that are subscribed to it then are
            evaluated in the next delta cycle, whether they subscribed before or after this call. The order
            in which a process that notifies and a process that waits are evaluated therefore does not
            matter. (SystemC: notify(SC_ZERO_TIME).)

            The event must stay alive until that delta cycle has started.
        */
        void notify();

        // True during the evaluation phase that follows an occurrence of the event, which is the one that
        // evaluates the processes it woke. A signal's event only occurs if a process is subscribed to it.
        bool has_happened() const;
        explicit operator bool() const { return has_happened(); }

    private:
        // The static and dynamic subscriber lists. Defined in event.cpp.
        struct Subscribers;

        /*
            notify() for a signal that commits a new value in the update phase. No process runs between the
            update phase and trigger(), so the subscribers are known here, and an event without any (most
            events) is not scheduled at all and does not "happen".

            Inline: a signal calls it up to twice for every change, and for most events it ends here.
        */
        void notify_committed()
        {
            if (subscribers_) [[unlikely]]
            {
                schedule_if_subscribed();
            }
        }
        // Schedule the event if a process is subscribed to it. The subscriber lists exist.
        void schedule_if_subscribed();

        // The event occurs: schedule the subscribed processes. Called by the scheduler for each notified event.
        void trigger();

        // Subscribe a process with static sensitivity: it is scheduled whenever the event occurs.
        void subscribe_static(ProcessBase *process);
        void unsubscribe_static(ProcessBase *process);
        // Subscribe a process with dynamic sensitivity: scheduled once, on the next occurrence.
        void subscribe_dynamic(ProcessBase *process);
        void unsubscribe_dynamic(ProcessBase *process);
        // Add the static subscribers of another event (ports hand theirs to the bound signal at elaboration).
        // Each process records this event as one of its static events.
        void merge_static_subscribers(const SensitivityEvent &other);

        bool has_static_subscribers() const;
        bool has_dynamic_subscribers() const;

        // The subscriber lists, allocated on first use.
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
        // The accessors and comparisons are inline: the scheduler's time queue uses them for every time step.
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
