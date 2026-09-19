#pragma once
#include <dspsim/forward.h>
#include <dspsim/event.h>
#include <vector>

namespace dspsim
{
    class Module;
    class SensitivityList
    {
    private:
        Module *_module;
        Context *_context;

    public:
        SensitivityList(Module *module);
        void add_event(SensitivityEvent &subscribers);

        Module *module() const { return _module; }

        SensitivityList &operator<<(SensitivityEvent &event);
    };
}