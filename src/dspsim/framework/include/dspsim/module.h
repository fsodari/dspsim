#pragma once
#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/port.h>
#include <dspsim/sensitivity_list.h>
#include <dspsim/module_name.h>

namespace dspsim
{
    class Module : public Model
    {
    public:
        SensitivityList always;

        Module(ModuleName &name);
        Module();
        void add_event(std::vector<Model *> &event_subscribers);

        void end_construction();

        // Python bindings will need to use this.
        template <typename M>
        static std::shared_ptr<M> create(const std::string &name)
        {
            ModuleName mname{name};
            return std::make_shared<M>(mname);
        }
    };

} // namespace dspsim

#define DSPSIM_CTOR(module_name) module_name(::dspsim::ModuleName)