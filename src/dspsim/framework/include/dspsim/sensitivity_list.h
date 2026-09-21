#pragma once
#include <dspsim/event.h>
#include <dspsim/process.h>
#include <vector>

namespace dspsim
{
    class Context;
    class Module;
    class SensitivityList
    {
    private:
        Context *_context;
        Module *_module;
        Process *_active_process;

    public:
        SensitivityList(Module *module);
        Module *module() const;

        void set_active_process(Process *process);
        Process *active_process() const;

        void link_process(SensitivityEvent &event, Process *process);
        void add_event(SensitivityEvent &event);
        SensitivityList &operator<<(SensitivityEvent &event);
    };
}