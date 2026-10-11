#include <dspsim/module.h>
#include <dspsim/context.h>
#include <dspsim/process.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace dspsim
{

    // Name of the innermost ModuleName scope, for modules constructed without a ModuleName argument.
    std::string Module::current_module_name()
    {
        auto &names = Context::obtain()->active_module_name_stack_;
        if (names.empty())
        {
            throw std::logic_error("Module() requires an active ModuleName. Construct the module with a ModuleName.");
        }
        return names.back();
    }

    // The running process, for next_trigger(event): a coroutine process must wait(event) instead.
    ProcessBase *Module::method_process(Context *context)
    {
        ProcessBase *process = context->current_process();
        if (process->is_coroutine()) [[unlikely]]
        {
            throw std::logic_error("next_trigger(event) is not allowed in coroutine process " + process->name() +
                                   ": use wait(event) instead");
        }
        return process;
    }

    Module::Module() : Model(current_module_name(), "module")
    {
        SPDLOG_LOGGER_DEBUG(context()->logger_, "Constructing Module with default name = {}", name());
        context()->active_module_stack_.push_back(this);
        context()->add_module(this);
    }

    Module::Module(ModuleName &name) : Model(name.name(), "module")
    {
        SPDLOG_LOGGER_DEBUG(context()->logger_, "Constructing Module with name = {}", name.name());
        context()->active_module_stack_.push_back(this);
        context()->add_module(this);
    }

    void Module::next_trigger(uint64_t time_delta)
    {
        context()->schedule_time_delta_event(time_delta);
    }

    void Module::next_trigger(SensitivityEvent &event)
    {
        method_process(context())->schedule_dynamic_event(event);
    }
    void Module::next_trigger(const std::string &event_name)
    {
        method_process(context())->schedule_dynamic_event(event_name);
    }

    WaitTimeEvent Module::wait(uint64_t time_delta)
    {
        return context()->wait(time_delta);
    }

    Wait Module::wait()
    {
        return context()->wait();
    }

    WaitSensitivityEvent Module::wait(SensitivityEvent &event)
    {
        return context()->wait(event);
    }

    WaitSensitivityEvent Module::wait(std::vector<std::reference_wrapper<SensitivityEvent>> events)
    {
        return context()->wait(events);
    }

    WaitSensitivityEvent Module::wait(const std::vector<SensitivityEvent *> &events)
    {
        return context()->wait(events);
    }

    WaitSensitivityEvent Module::wait(SensitivityEvent &event, uint64_t timeout)
    {
        return context()->wait(event, timeout);
    }

    WaitSensitivityEvent Module::wait(std::vector<std::reference_wrapper<SensitivityEvent>> events, uint64_t timeout)
    {
        return context()->wait(events, timeout);
    }

    WaitSensitivityEvent Module::wait(const std::vector<SensitivityEvent *> &events, uint64_t timeout)
    {
        return context()->wait(events, timeout);
    }

    void Module::end_construction()
    {
        context()->active_module_stack_.pop_back();
    }

    void Module::add_port(PortBase *port)
    {
        ports_.push_back(port);
        if (port->kind() == "input")
        {
            inputs_.push_back(port);
        }
        else if (port->kind() == "output")
        {
            outputs_.push_back(port);
        }
    }
} // namespace dspsim
