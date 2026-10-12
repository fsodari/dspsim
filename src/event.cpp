#include <dspsim/event.h>
#include <dspsim/context.h>
#include <dspsim/process.h>
#include <dspsim/utils/inlining.h>

#include <algorithm>
#include <cstdint>

namespace dspsim
{
    namespace detail
    {
        /*
            The processes subscribed to an event, in the order they subscribed.

            Most events have one or two subscribers. Those are stored in the list itself, next to its size,
            so that the scheduler finds them in the cache line it reads anyway: when an event occurs, the
            path from the event to the process it wakes is a chain of loads that each wait for the one
            before, and a design whose processes wake each other in turn runs at the speed of that chain.

            There is no check for duplicates. A flag per process id used to reject them, which made every
            event with a subscriber as large as the id of that subscriber: the memory of a design grew with
            the square of its number of processes. Duplicates are prevented, or harmless, without it:

            - ProcessBase::schedule_static_event() subscribes a process to an event once.
            - A process that waits on the same event twice is in the dynamic list twice. It records the
              event twice as well, so every entry is removed again, and the scheduling stack ignores the
              second attempt to schedule the process.
        */
        template <uint32_t INLINE>
        class SubscriberList
        {
        public:
            SubscriberList() = default;
            SubscriberList(const SubscriberList &) = delete;
            SubscriberList &operator=(const SubscriberList &) = delete;
            ~SubscriberList()
            {
                if (data_ != inline_)
                {
                    delete[] data_;
                }
            }

            bool empty() const { return size_ == 0; }
            ProcessBase **begin() { return data_; }
            ProcessBase **end() { return data_ + size_; }
            ProcessBase *back() const { return data_[size_ - 1]; }
            void pop_back() { --size_; }

            void push_back(ProcessBase *process)
            {
                if (size_ == capacity_) [[unlikely]]
                {
                    grow();
                }
                data_[size_++] = process;
            }

            // Remove the first entry for the process, if there is one. Keeps the order of the others.
            // Written out as loops: the list usually has one entry, and a call to move nothing is not free.
            void erase_first(ProcessBase *process)
            {
                for (uint32_t i = 0; i < size_; ++i)
                {
                    if (data_[i] == process)
                    {
                        for (uint32_t next = i + 1; next < size_; ++next)
                        {
                            data_[next - 1] = data_[next];
                        }
                        --size_;
                        return;
                    }
                }
            }

            // Remove every entry for the process.
            void erase_all(ProcessBase *process)
            {
                size_ = static_cast<uint32_t>(std::remove(begin(), end(), process) - begin());
            }

        private:
            // Not inlined: push_back() is called for every dynamic wait, and has to stay small.
            DSPSIM_NOINLINE void grow()
            {
                const uint32_t capacity = capacity_ * 2;
                ProcessBase **data = new ProcessBase *[capacity];
                std::copy(begin(), end(), data);
                if (data_ != inline_)
                {
                    delete[] data_;
                }
                data_ = data;
                capacity_ = capacity;
            }

        private:
            ProcessBase **data_ = inline_;
            uint32_t size_ = 0;
            uint32_t capacity_ = INLINE;
            ProcessBase *inline_[INLINE];
        };
    } // namespace detail

    // One cache line: the scheduler reads all of it when the event occurs.
    struct alignas(64) SensitivityEvent::Subscribers
    {
        // Processes with static sensitivity are notified whenever this event occurs unless they are waiting on a dynamic event.
        detail::SubscriberList<2> static_;
        // Processes with dynamic sensitivity. This list is cleared when the event occurs, each process is removed
        // from its other dynamic events, and it returns to having static sensitivity.
        detail::SubscriberList<1> dynamic_;
        // Set by notify() until the event occurs, so that it occurs once however often it is notified.
        bool notified_ = false;
    };
    SensitivityEvent::SensitivityEvent(Context *context)
        : context_(context),
          change_time_{-1},
          subscribers_(nullptr)
    {
    }

    SensitivityEvent::~SensitivityEvent() = default;

    SensitivityEvent::Subscribers &SensitivityEvent::subscribers()
    {
        static_assert(sizeof(Subscribers) == 64, "the subscriber lists of an event fill one cache line");
        if (!subscribers_)
        {
            subscribers_ = std::make_unique<Subscribers>();
        }
        return *subscribers_;
    }

    void SensitivityEvent::notify()
    {
        // Who is subscribed is only known once the evaluation phase is over: a process evaluated after the
        // caller may still start to wait on this event. trigger() looks at the subscribers.
        Subscribers &subs = subscribers();
        // A queue or a channel may notify the same event for every item it handles. Schedule it once.
        if (!subs.notified_)
        {
            subs.notified_ = true;
            context_->sensitivity_event_stack_.push_back(this);
        }
    }

    void SensitivityEvent::schedule_if_subscribed()
    {
        if (!subscribers_->static_.empty() || !subscribers_->dynamic_.empty())
        {
            context_->sensitivity_event_stack_.push_back(this);
        }
    }

    void SensitivityEvent::trigger()
    {
        // The subscribers are evaluated in the next evaluation phase, which has this update count.
        change_time_ = context_->update_count();
        // Only notified events are triggered, and those have subscriber lists.
        Subscribers &subs = *subscribers_;
        subs.notified_ = false;
        // Schedule processes with static sensitivity if they are not currently waiting on a dynamic event.
        for (ProcessBase *process : subs.static_)
        {
            if (!process->static_sensitivity_disabled())
            {
                context_->process_eval_stack_.push_back(process);
            }
        }
        /*
            Schedule the processes that wait on the event, which ends their wait.

            This function has to stay small: the scheduler calls it for every event that occurs, and the
            compiler inlines it there only while it is small. So the usual case, a process that waits on
            this event alone, is handled here, and the rest is a call (dynamic_event_triggered()).
        */
        while (!subs.dynamic_.empty())
        {
            ProcessBase *process = subs.dynamic_.back();
            subs.dynamic_.pop_back();
            context_->process_eval_stack_.push_back(process);
            if (process->dynamic_events_.size() == 1) [[likely]]
            {
                process->dynamic_events_.clear();
                process->reset_static_sensitivity();
            }
            else
            {
                // Unsubscribe from the process's other dynamic events and restore its static sensitivity.
                process->dynamic_event_triggered(*this);
            }
        }
    }

    bool SensitivityEvent::has_happened() const
    {
        return change_time_ == context_->update_count();
    }

    void SensitivityEvent::subscribe_static(ProcessBase *process)
    {
        subscribers().static_.push_back(process);
    }

    void SensitivityEvent::unsubscribe_static(ProcessBase *process)
    {
        if (subscribers_)
        {
            subscribers_->static_.erase_all(process);
        }
    }

    void SensitivityEvent::subscribe_dynamic(ProcessBase *process)
    {
        subscribers().dynamic_.push_back(process);
    }

    void SensitivityEvent::unsubscribe_dynamic(ProcessBase *process)
    {
        if (subscribers_)
        {
            // One subscription: the process calls this once for each time it subscribed.
            subscribers_->dynamic_.erase_first(process);
        }
    }

    void SensitivityEvent::merge_static_subscribers(const SensitivityEvent &other)
    {
        if (other.subscribers_)
        {
            // Subscribe through the process so that it can unsubscribe from this event when it completes.
            for (auto *process : other.subscribers_->static_)
            {
                process->schedule_static_event(*this);
            }
        }
    }

    bool SensitivityEvent::has_static_subscribers() const
    {
        return subscribers_ && !subscribers_->static_.empty();
    }

    bool SensitivityEvent::has_dynamic_subscribers() const
    {
        return subscribers_ && !subscribers_->dynamic_.empty();
    }

    TimeEvent::TimeEvent(uint64_t time_update, ProcessBase *process, uint64_t wake_count)
        : time_update_(time_update),
          process_(process),
          wake_count_(wake_count)
    {
    }
}
