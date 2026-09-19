#include <dspsim/sensitivity_list.h>
#include <dspsim/module.h>
#include "internal.h"

namespace dspsim
{
    SensitivityList::SensitivityList(Module *module)
        : _module(module), _context(Context::obtain().get())
    {
    }

    Module *SensitivityList::module() const
    {
        return _module;
    }

    void SensitivityList::add_event(SensitivityEvent &event)
    {
        _context->logger->debug("Adding event for module: {}", _module->name());
        event.add_subscriber(_module);
    }
    SensitivityList &SensitivityList::operator<<(SensitivityEvent &event)
    {
        add_event(event);
        return *this;
    }
}