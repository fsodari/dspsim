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
} // namespace dspsim
