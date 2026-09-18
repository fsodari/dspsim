#include <dspsim/module.h>

namespace dspsim
{

    Module::Module() : Module(*Context::obtain()->_active_module_name_stack.back())
    {
    }
    Module::Module(ModuleName &name) : Model(name.name()), always(this)
    {
        context()->_active_module_stack.push_back(this);
    }

    void Module::add_event(std::vector<Model *> &event_subscribers)
    {
        event_subscribers.push_back(this);
    }

    void Module::end_construction()
    {
        context()->_active_module_stack.pop_back();
    }
} // namespace dspsim
