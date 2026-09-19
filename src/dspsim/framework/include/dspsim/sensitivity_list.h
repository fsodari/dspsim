#pragma once
#include <dspsim/context.h>
#include <dspsim/event.h>
#include <vector>

namespace dspsim
{
    class SensitivityList
    {
    private:
        Module *_module;
        Context *_context;

    public:
        SensitivityList(Module *module);
        Module *module() const;

        void add_event(SensitivityEvent &event);
        SensitivityList &operator<<(SensitivityEvent &event);
    };
}