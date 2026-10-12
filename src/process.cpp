#include <dspsim/process.h>
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <dspsim/module.h>
#include <dspsim/port.h>

#include "log.h"

#include <algorithm>
#include <stdexcept>

namespace dspsim
{
    ProcessBase::ProcessBase(Context *context, const std::string &name, bool coroutine)
        : context_(context),
          id_(context->next_process_id()),
          coroutine_(coroutine),
          name_(name),
          parent_module_(context->active_module())
    {
    }

    ProcessBase::~ProcessBase() = default;

    const std::string &ProcessBase::name() const
    {
        return name_;
    }

    bool ProcessBase::initialize() const
    {
        return initialize_;
    }

    ProcessBase *ProcessBase::initialize(bool init)
    {
        if (init && coroutine_ && done())
        {
            // A completed coroutine cannot be resumed.
            throw std::logic_error("Cannot initialize process " + name_ + ": the coroutine has completed");
        }
        initialize_ = init;
        if (init)
        {
            context_->process_eval_stack_.push_back(this);
        }
        else
        {
            context_->process_eval_stack_.erase(this);
        }
        return this;
    }

    ProcessBase *ProcessBase::disable()
    {
        if (coroutine_ && static_sensitivity_ == StaticSensitivity::Waiting)
        {
            throw std::logic_error("Cannot disable process " + name_ +
                                   ": the coroutine is waiting for an event or a time and would lose that wait. "
                                   "Only a coroutine that waits on its static sensitivity can be disabled");
        }
        // A method process: drop the triggers it has asked for with next_trigger().
        cancel_dynamic_events();
        if (time_events_used_)
        {
            context_->time_event_stack_.erase_if([this](const TimeEvent &event)
                                                 { return event.process() == this; });
        }
        static_sensitivity_ = StaticSensitivity::Disabled;
        context_->process_eval_stack_.erase(this);
        return this;
    }

    ProcessBase *ProcessBase::enable()
    {
        if (static_sensitivity_ == StaticSensitivity::Disabled)
        {
            static_sensitivity_ = StaticSensitivity::Active;
        }
        return this;
    }

    void ProcessBase::complete()
    {
        unschedule();
        if (detached_)
        {
            context_->completed_processes_.push_back(this);
        }
    }

    void ProcessBase::unschedule()
    {
        cancel_static_events();
        cancel_dynamic_events();
        // Pending time waits are tagged with the old wake count.
        ++wake_count_;
    }

    void ProcessBase::schedule_static_event(SensitivityEvent &event)
    {
        // Subscribe to each event once. It may be named twice: always(a, a), "*" together with an input,
        // or two ports that are bound to the same signal.
        if (std::find(static_events_.begin(), static_events_.end(), &event) == static_events_.end())
        {
            static_events_.push_back(&event);
            event.subscribe_static(this);
        }
    }

    void ProcessBase::schedule_static_event(const std::string &event_name)
    {
        // Implementation goes here
        // Make the sensitivity list sensitive to all events.
        if (event_name == "*")
        {
            for (auto &input : parent_module_->inputs())
            {
                schedule_static_event(input->change());
            }
        }
        else
        {
            DSPSIM_LOG_ERROR(context_->logger_, "Event not found: " + event_name);
        }
    }

    void ProcessBase::schedule_dynamic_event(SensitivityEvent &event)
    {
        // One entry for each subscription, with no check for an event that is already in the list: a
        // search here made a wait on n events quadratic in n. Whatever ends the wait removes the
        // subscriptions one by one, so an event that is waited on twice is handled like two events.
        event.subscribe_dynamic(this);
        dynamic_events_.push_back(&event);
        static_sensitivity_ = StaticSensitivity::Waiting;
    }
    void ProcessBase::schedule_dynamic_event(const std::string &event_name)
    {
        if (event_name == "*")
        {
            for (auto &input : parent_module_->inputs())
            {
                schedule_dynamic_event(input->change());
            }
        }
        else
        {
            DSPSIM_LOG_ERROR(context_->logger_, "Event not found: " + event_name);
        }
    }

    void ProcessBase::schedule_time_wait(uint64_t time_delta)
    {
        if (time_delta == 0) [[unlikely]]
        {
            // A wait of no time ends in the next delta cycle.
            context_->wait_for_next_delta(this);
            return;
        }
        // Tagged with the wake count: if anything else resumes the process first, the wakeup is stale.
        context_->time_event_stack_.emplace(context_->time() + time_delta, this, wake_count_);
        static_sensitivity_ = StaticSensitivity::Waiting;
        time_events_used_ = true;
    }

    void ProcessBase::dynamic_event_triggered(SensitivityEvent &event)
    {
        // The triggering event removes this process from its own list. Remove it from the others.
        for (auto *other : dynamic_events_)
        {
            if (other != &event)
            {
                other->unsubscribe_dynamic(this);
            }
        }
        dynamic_events_.clear();
        reset_static_sensitivity();
    }

    void ProcessBase::time_wait_triggered()
    {
        cancel_dynamic_events();
        reset_static_sensitivity();
    }

    void ProcessBase::cancel_static_events()
    {
        for (auto *event : static_events_)
        {
            event->unsubscribe_static(this);
        }
        static_events_.clear();
    }

    void ProcessBase::cancel_dynamic_events()
    {
        for (auto *event : dynamic_events_)
        {
            event->unsubscribe_dynamic(this);
        }
        dynamic_events_.clear();
    }

    Process::Process(Context *context, std::function<void()> eval, const std::string &name)
        : ProcessBase(context, name), eval_(eval)
    {
    }

    bool Process::done() const
    {
        return done_;
    }

    void Process::resume()
    {
        eval_();
        done_ = true;
    }

    MethodProcess::MethodProcess(Context *context, void *instance, Trampoline call, const std::string &name)
        : ProcessBase(context, name), instance_(instance), call_(call)
    {
    }

    bool MethodProcess::done() const
    {
        return done_;
    }

    void MethodProcess::resume()
    {
        call_(instance_);
        done_ = true;
    }

    /*
        Coroutine processes.
    */
    CoroProcess::CoroProcess(Context *context, std::coroutine_handle<> root, const std::string &name)
        : ProcessBase(context, name, true), root_(root)
    {
        set_resume_point(root_);
    }

    CoroProcess::~CoroProcess()
    {
        if (root_)
        {
            root_.destroy();
        }
    }

    bool CoroProcess::done() const
    {
        return !root_ || root_.done();
    }

    void CoroProcess::resume()
    {
        try
        {
            resume_point().resume();
        }
        catch (...)
        {
            // An exception leaves the root task at its final suspend point.
            complete();
            throw;
        }
        if (root_.done()) [[unlikely]]
        {
            // The task has completed: nothing may schedule it again.
            complete();
        }
    }
}
