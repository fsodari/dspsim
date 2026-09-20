#pragma once
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <vector>

namespace dspsim
{
    class VModuleBase : public Module
    {
    public:
        std::vector<InputBase *> _inputs;
        std::vector<OutputBase *> _outputs;

        void _sync_inputs() const;
        void _sync_outputs() const;

    public:
        VModuleBase(ModuleName &name);
    };
} // namespace dspsim