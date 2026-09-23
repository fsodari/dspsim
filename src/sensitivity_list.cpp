#include <dspsim/sensitivity_list.h>
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <dspsim/process.h>
#include <dspsim/module.h>

#include "internal.h"

namespace dspsim
{
    SensitivityList::SensitivityList()
        : _context(Context::obtain().get())
    {
    }

    void SensitivityList::link_process(SensitivityEvent *event, Process *process)
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
        event->add_process(process);
    }

    SensitivityList &SensitivityList::operator<<(SensitivityEvent *event)
    {
        link_process(event, nullptr);
        return *this;
    }

    void SensitivityList::operator()(const std::string &event_name)
    {
        if (event_name == "*")
        {
            // Make the sensitivity list sensitive to all events.
            for (auto &input : _context->_active_module()->_inputs)
            {
                link_process(input->_change(), nullptr);
            }
        }
    }
}