#pragma once
#include <dspsim/module.h>

namespace dspsim
{
    class VModuleBase : public Module
    {
    public:
        std::vector<OutputBase *> _outputs;

        void _sync_outputs() const;

    public:
        VModuleBase(ModuleName &name);
    };
} // namespace dspsim