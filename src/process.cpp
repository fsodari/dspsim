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
        event.static_subscribers().push_back(this);
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
        event.dynamic_subscribers().push_back(this);
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
                other->dynamic_subscribers().erase(this);
            }
        }
        dynamic_events_.clear();
        reset_static_sensitivity();
    }

    Process::Process(Context *context, std::function<void()> eval, const std::string &name)
        : ProcessBase(context, name), eval_(eval)
    {
    }

    /*
        Coroutine processes.
    */
    CoroProcess::CoroProcess(Context *context, Task task, const std::string &name)
        : ProcessBase(context, name), task_(std::move(task))
    {
    }
    void CoroProcess::resume()
    {
        if (!task_.handle.done()) [[likely]]
        {
            task_.handle.resume();
        }
        else
        {
            // Coroutine has completed and cannot be resumed.
            context()->logger->error("Attempted to resume a completed coroutine! {}", name());
        }
    }
    bool CoroProcess::done() const
    {
        return !task_.handle || task_.handle.done();
    }
}