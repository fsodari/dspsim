#include <dspsim/vmodule/vmodule_base.h>
#include <dspsim/port.h>
#include <spdlog/spdlog.h>

namespace dspsim
{
    VModuleBase::VModuleBase(ModuleName &name) : Module(name)
    {
        context()->logger->info("Initializing VModuleBase: {}", this->name());
    }

    void VModuleBase::_sync_outputs() const
    {
        for (auto output : _outputs)
        {
            output->_notify(EventType::Changed);
        }
    }
}