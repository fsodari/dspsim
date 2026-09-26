#include <dspsim/process.h>
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <dspsim/module.h>

#include <spdlog/spdlog.h>

namespace dspsim
{

    Process::Process(uint32_t id, std::function<void()> eval, Model *source, const std::string &name)
        : _context(Context::obtain().get()), _source(source), _id(id), eval(eval), _name(name)
    {
    }

    void Process::link_event(SensitivityEvent *event)
    {
        // Implementation goes here
        event->add_process(this);
    }

    void Process::link_event(const std::string &event_name)
    {
        // Implementation goes here
        // Make the sensitivity list sensitive to all events.
        if (event_name == "*")
        {
            for (auto &input : _context->_active_module()->inputs())
            {
                link_event(input->_change());
            }
        }
        else
        {
            _context->logger->error("Event not found: {}", event_name);
        }
    }
}