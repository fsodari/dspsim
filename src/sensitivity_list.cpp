#include <dspsim/sensitivity_list.h>
#include <dspsim/module.h>
#include "internal.h"

namespace dspsim
{
    SensitivityList::SensitivityList(Module *module)
        : _module(module), _context(Context::obtain())
    {
    }

    void SensitivityList::add_event(std::vector<Module *> &subscribers)
    {
        _context->logger->debug("Adding event for module: {}", _module->name());
        subscribers.push_back(_module);
    }
    SensitivityList &SensitivityList::operator<<(std::vector<Module *> &event)
    {
        add_event(event);
        return *this;
    }
    SensitivityList &SensitivityList::operator<<(InputBase &port)
    {
        add_event(port._changed_subscribers);
        return *this;
    }
}