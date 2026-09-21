#include <dspsim/vmodule/vmodule_base.h>
#include <dspsim/context.h>
#include <dspsim/port.h>
#include <spdlog/spdlog.h>

namespace dspsim
{
    VModuleBase::VModuleBase(ModuleName &name) : Module(name)
    {
        context()->logger->info("Initializing VModuleBase: {}", this->name());
    }

    void VModuleBase::_sync_inputs() const
    {
        for (auto input : _inputs)
        {
            input->_sync();
        }
    }
    void VModuleBase::_sync_outputs() const
    {
        for (auto output : _outputs)
        {
            output->_sync();
        }
    }
}