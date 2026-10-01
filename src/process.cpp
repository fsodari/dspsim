#include <dspsim/process.h>
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <dspsim/module.h>

#include <spdlog/spdlog.h>

namespace dspsim
{
    ProcessBase::ProcessBase(const std::string &name)
        : context_(Context::obtain().get()),
          id_(context()->next_process_id()),
          name_(name),
          parent_module_(context()->_active_module())
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
        // Implementation goes here
        event.dynamic_subscribers().push_back(this);
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

    Process::Process(std::function<void()> eval, const std::string &name)
        : ProcessBase(name), eval_(eval)
    {
    }

    /*
        Coroutine processes.
    */
    CoroProcess::CoroProcess(Task task, const std::string &name)
        : ProcessBase(name), task_(std::move(task))
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