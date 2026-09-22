#include <dspsim/sensitivity_list.h>
#include <dspsim/context.h>
#include <dspsim/process.h>
#include "internal.h"

namespace dspsim
{
    SensitivityList::SensitivityList()
        : _context(Context::obtain().get())
    {
    }

    void SensitivityList::link_process(SensitivityEvent &event, Process *process)
    {
        if (process == nullptr)
        {
            process = _context->_active_process;
        }
        if (process == nullptr)
        {
            _context->logger->error("No active process to link for event. Current Hierarchy: {}", _context->_current_hierarchy());
            return;
        }
        event.add_process(process);
    }

    SensitivityList &SensitivityList::operator<<(SensitivityEvent &event)
    {
        link_process(event, nullptr);
        return *this;
    }
}