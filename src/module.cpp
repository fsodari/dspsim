#include <dspsim/module.h>
#include <spdlog/spdlog.h>

namespace dspsim
{

    Module::Module() : Module(*Context::obtain()->_active_module_name_stack.back())
    {
    }
    Module::Module(ModuleName &name) : Model(name.name(), "module"), always(this)
    {
        SPDLOG_LOGGER_TRACE(context()->logger, "Constructing Module with name = {}", name.name());
        context()->_active_module_stack.push_back(this);
    }

    void Module::_add_event(std::vector<Model *> &event_subscribers)
    {
        event_subscribers.push_back(this);
    }

    void Module::_end_construction()
    {
        context()->_active_module_stack.pop_back();
    }

    void Module::dont_initialize() { _initialize = false; }
    void Module::_set_initialize(bool value) { _initialize = value; }
    bool Module::initialize() const { return _initialize; }
} // namespace dspsim
