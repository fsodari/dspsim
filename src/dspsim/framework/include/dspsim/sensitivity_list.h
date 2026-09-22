#pragma once
#include <dspsim/event.h>
// #include <dspsim/process.h>
#include <vector>

namespace dspsim
{
    class Context;
    class Process;
    class SensitivityList
    {
    private:
        Context *_context;

    public:
        SensitivityList();

        // Link a process to a sensitivity event. This will ensure the process is triggered when the event occurs.
        // If a nullptr is passed, it will use the context's active process.
        void link_process(SensitivityEvent &event, Process *process = nullptr);

        SensitivityList &operator<<(SensitivityEvent &event);
    };
}