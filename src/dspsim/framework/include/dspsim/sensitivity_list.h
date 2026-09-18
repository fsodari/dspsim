#pragma once
// #include <dspsim/module.h>
// #include <dspsim/event.h>
// #include <dspsim/module.h>
#include <dspsim/port.h>
#include <vector>

namespace dspsim
{
    class Module;
    class SensitivityList
    {
    private:
        Module *_module;
        ContextPtr _context;

    public:
        SensitivityList(Module *module);
        void add_event(std::vector<Module *> &subscribers);

        SensitivityList &operator<<(std::vector<Module *> &event);
        SensitivityList &operator<<(InputBase &port);
    };
}