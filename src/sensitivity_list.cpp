#include <dspsim/sensitivity_list.h>
#include <dspsim/context.h>
#include <dspsim/module.h>
#include "internal.h"

namespace dspsim
{
    SensitivityList::SensitivityList(Module *module)
        : _context(Context::obtain().get()), _module(module), _active_process(nullptr)
    {
    }

    Module *SensitivityList::module() const
    {
        return _module;
    }

    void SensitivityList::set_active_process(Process *process)
    {
        _active_process = process;
    }

    Process *SensitivityList::active_process() const
    {
        return _active_process;
    }

    void SensitivityList::link_process(SensitivityEvent &event, Process *process)
    {
        _context->logger->debug("Linking process for module: {}", _module->name());
        // event.add_subscriber(_module);
        event.add_process(process);
    }

    void SensitivityList::add_event(SensitivityEvent &event)
    {
        _context->logger->debug("Adding event for module: {}", _module->name());
        event.add_subscriber(_module);
        if (_active_process)
        {
            _context->logger->info("Adding process for module: {}", _module->name());
            event.add_process(_active_process);
        }
    }
    SensitivityList &SensitivityList::operator<<(SensitivityEvent &event)
    {
        add_event(event);
        return *this;
    }
}