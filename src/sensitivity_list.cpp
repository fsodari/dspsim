#include <dspsim/sensitivity_list.h>
#include <dspsim/module.h>
#include "internal.h"

namespace dspsim
{
    SensitivityList::SensitivityList(Module *module)
        : _module(module), _context(Context::obtain().get())
    {
    }

    void SensitivityList::add_event(SensitivityEvent &subscribers)
    {
        _context->logger->debug("Adding event for module: {}", _module->name());
        subscribers.push_back(_module);
    }
    SensitivityList &SensitivityList::operator<<(SensitivityEvent &event)
    {
        add_event(event);
        return *this;
    }
}