#pragma once
#include <dspsim/model.h>
#include <dspsim/sensitivity_list.h>
#include <dspsim/module_name.h>
#include <vector>

namespace dspsim
{
    class SensitivityList;
    class Module : public Model
    {
    private:
        bool _initialize = true;

    public:
        SensitivityList always;
        // Used by python.
        SensitivityList &_always_ref();

        Module(ModuleName &name);
        Module();

        void _add_event(std::vector<Model *> &event_subscribers);
        void _end_construction();

        // Opt out of the initial evaluation pass Context::elaborate() otherwise schedules for every module.
        void dont_initialize();
        void _set_initialize(bool value);
        bool initialize() const;
    };

} // namespace dspsim

#define DSPSIM_CTOR(module_name) module_name(::dspsim::ModuleName)