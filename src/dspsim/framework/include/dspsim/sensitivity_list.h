#pragma once
#include <string>

namespace dspsim
{
    class Context;
    class Process;
    class SensitivityEvent;
    class SensitivityList
    {
    private:
        Context *_context;

    public:
        SensitivityList();

        // Link a process to a sensitivity event. This will ensure the process is triggered when the event occurs.
        // If a nullptr is passed, it will use the context's active process.
        void link_process(SensitivityEvent *event, Process *process = nullptr);
        void link_process(const std::string &event_name) { link_process_str(event_name); }
        void _link_process(SensitivityEvent *event, Process *process = nullptr) { link_process(event, process); }

        // Allow ("*") to be sensitive to all events. Or specify inputs by name maybe?
        void link_process_str(const std::string &event_name);

        SensitivityList &operator<<(SensitivityEvent *event);

        // Allow ("*") to be sensitive to all events. Or specify inputs by name maybe?
        void operator()(const std::string &event_name);

        template <typename... Args>
        void operator()(Args &&...args)
        {
            // The comma operator executes print_item for each argument in sequence
            (link_process(std::forward<Args>(args)), ...);
        }
    };
}