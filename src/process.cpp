#include <dspsim/process.h>
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <dspsim/module.h>

#include <spdlog/spdlog.h>

namespace dspsim
{
    ProcessBase::ProcessBase(const std::string &name)
        : _context(Context::obtain().get()),
          _id(_context->next_process_id()),
          _name(name),
          _parent_module(_context->_active_module())
    {
    }

    void ProcessBase::schedule_static_event(SensitivityEvent *event)
    {
        event->static_subscribers().push_back(this);
    }

    void ProcessBase::schedule_static_event(const std::string &event_name)
    {
        // Implementation goes here
        // Make the sensitivity list sensitive to all events.
        if (event_name == "*")
        {
            for (auto &input : _parent_module->inputs())
            {
                schedule_static_event(input->_change());
            }
        }
        else
        {
            _context->logger->error("Event not found: {}", event_name);
        }
    }

    void ProcessBase::schedule_dynamic_event(SensitivityEvent *event)
    {
        // Implementation goes here
        event->dynamic_subscribers().push_back(this);
        this->_static_sensitivity_disabled = true;
    }
    void ProcessBase::schedule_dynamic_event(const std::string &event_name)
    {
        if (event_name == "*")
        {
            for (auto &input : _parent_module->inputs())
            {
                schedule_dynamic_event(input->change_event());
            }
        }
        else
        {
            _context->logger->error("Event not found: {}", event_name);
        }
    }

    Process::Process(std::function<void()> eval, const std::string &name)
        : ProcessBase(name), _eval(eval)
    {
    }

    /*
        Coroutine processes.
    */
    CoroProcess::CoroProcess(Task task, const std::string &name)
        : ProcessBase(name), _task(std::move(task))
    {
    }
    void CoroProcess::resume()
    {
        if (!_task.handle.done()) [[likely]]
        {
            _task.handle.resume();
        }
        else
        {
            // Coroutine has completed and cannot be resumed.
            context()->logger->error("Attempted to resume a completed coroutine! {}", name());
        }
    }
}