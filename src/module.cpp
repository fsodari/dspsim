#include <dspsim/module.h>
#include <dspsim/context.h>
#include <spdlog/spdlog.h>

namespace dspsim
{

    Module::Module() : Module(*Context::obtain()->_active_module_name_stack.back())
    {
        SPDLOG_LOGGER_DEBUG(context()->logger, "Constructing Module with default name = {}", context()->_active_module_name_stack.back()->name());
    }

    Module::Module(ModuleName &name) : Model(name.name(), "module")
    {
        SPDLOG_LOGGER_DEBUG(context()->logger, "Constructing Module with name = {}", name.name());
        context()->_active_module_stack.push_back(this);
        context()->_add_module(this);
    }

    void Module::_end_construction()
    {
        context()->_active_module_stack.pop_back();
    }

    void Module::next_trigger(uint64_t time_delta, ProcessBase *process)
    {
        context()->schedule_time_delta_event(time_delta, process);
    }

    void Module::next_trigger(SensitivityEvent &event, ProcessBase *process)
    {
        if (process == nullptr)
        {
            process = context()->_current_process;
        }
        process->schedule_dynamic_event(event);
    }
    void Module::next_trigger(const std::string &event_name, ProcessBase *process)
    {
        if (process == nullptr)
        {
            process = context()->_current_process;
        }
        process->schedule_dynamic_event(event_name);
    }

    WaitTimeEvent Module::wait(uint64_t time_delta, ProcessBase *process)
    {
        return context()->wait(time_delta, process);
    }

    WaitSensitivityEvent Module::wait(ProcessBase *process)
    {
        return context()->wait(process);
    }

    WaitSensitivityEvent Module::wait(SensitivityEvent &event, ProcessBase *process)
    {
        return context()->wait(event, process);
    }

    WaitSensitivityEvent Module::wait(std::initializer_list<std::reference_wrapper<SensitivityEvent>> events, ProcessBase *process)
    {
        return context()->wait(events, process);
    }

} // namespace dspsim
