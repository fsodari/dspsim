#include <dspsim/process.h>
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <dspsim/module.h>

#include <spdlog/spdlog.h>
#include <algorithm>

namespace dspsim
{
    ProcessBase::ProcessBase(Context *context, const std::string &name)
        : context_(context),
          id_(context->next_process_id()),
          name_(name),
          parent_module_(context->_active_module())
    {
    }

    void ProcessBase::reset_static_sensitivity()
    {
        static_sensitivity_disabled_ = false;
    }

    void ProcessBase::schedule_static_event(SensitivityEvent &event)
    {
        event.subscribe_static(this);
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
            context_->logger->error("Event not found: {}", event_name);
        }
    }

    void ProcessBase::schedule_dynamic_event(SensitivityEvent &event)
    {
        event.subscribe_dynamic(this);
        // Record each event once. The list is short, usually a single event.
        if (std::find(dynamic_events_.begin(), dynamic_events_.end(), &event) == dynamic_events_.end())
        {
            dynamic_events_.push_back(&event);
        }
        this->static_sensitivity_disabled_ = true;
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
            context_->logger->error("Event not found: {}", event_name);
        }
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

    void ProcessBase::cancel_dynamic_events()
    {
        for (auto *event : dynamic_events_)
        {
            event->unsubscribe_dynamic(this);
        }
        dynamic_events_.clear();
    }

    void ProcessBase::schedule_time_wait(uint64_t time_delta)
    {
        // Tagged with the wake count: if anything else resumes the process first, the wakeup is stale.
        context_->_time_event_stack.emplace(context_->time() + time_delta, this, wake_count_);
        static_sensitivity_disabled_ = true;
    }

    void ProcessBase::time_wait_triggered()
    {
        cancel_dynamic_events();
        reset_static_sensitivity();
    }

    Process::Process(Context *context, std::function<void()> eval, const std::string &name)
        : ProcessBase(context, name), eval_(eval)
    {
    }

    MethodProcess::MethodProcess(Context *context, void *instance, Trampoline call, const std::string &name)
        : ProcessBase(context, name), instance_(instance), call_(call)
    {
    }

    /*
        Coroutine processes.
    */
    CoroProcess::CoroProcess(Context *context, std::coroutine_handle<> root, const std::string &name)
        : ProcessBase(context, name), root_(root)
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

    void CoroProcess::resume()
    {
        if (!root_.done()) [[likely]]
        {
            resume_point().resume();
        }
        else
        {
            // Coroutine has completed and cannot be resumed.
            context()->logger->error("Attempted to resume a completed coroutine! {}", name());
        }
    }
    bool CoroProcess::done() const
    {
        return !root_ || root_.done();
    }
}
