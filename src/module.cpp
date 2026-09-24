#include <dspsim/module.h>
#include <dspsim/context.h>
#include <spdlog/spdlog.h>

namespace dspsim
{

    Module::Module() : Module(*Context::obtain()->_active_module_name_stack.top())
    {
        context()->_add_module(this);
    }
    Module::Module(ModuleName &name) : Model(name.name(), "module")
    {
        SPDLOG_LOGGER_TRACE(context()->logger, "Constructing Module with name = {}", name.name());
        context()->_active_module_stack.push(this);
    }

    void Module::_end_construction()
    {
        context()->_active_module_stack.pop();
    }

    void Module::dont_initialize() { _initialize = false; }
    void Module::_set_initialize(bool value) { _initialize = value; }
    bool Module::initialize() const { return _initialize; }
} // namespace dspsim
