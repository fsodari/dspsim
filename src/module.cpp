#include <dspsim/module.h>
#include <dspsim/context.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace dspsim
{

    // Name of the innermost ModuleName scope, for modules constructed without a ModuleName argument.
    static std::string current_module_name()
    {
        auto &names = Context::obtain()->_active_module_name_stack;
        if (names.empty())
        {
            throw std::logic_error("Module() requires an active ModuleName. Construct the module with a ModuleName.");
        }
        return names.back();
    }

    Module::Module() : Model(current_module_name(), "module")
    {
        SPDLOG_LOGGER_DEBUG(context()->logger, "Constructing Module with default name = {}", name());
        context()->_active_module_stack.push_back(this);
        context()->_add_module(this);
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

    void Module::next_trigger(uint64_t time_delta)
    {
        context()->schedule_time_delta_event(time_delta);
    }

    void Module::next_trigger(SensitivityEvent &event)
    {
        context()->current_process()->schedule_dynamic_event(event);
    }
    void Module::next_trigger(const std::string &event_name)
    {
        context()->current_process()->schedule_dynamic_event(event_name);
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

} // namespace dspsim
