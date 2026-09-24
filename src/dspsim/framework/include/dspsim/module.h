#pragma once
#include <dspsim/model.h>
#include <dspsim/module_name.h>
#include <dspsim/process.h>
#include <dspsim/port.h>
#include <vector>

namespace dspsim
{
    class SensitivityList;
    class Module : public Model
    {
    private:
        bool _initialize = true;

    protected:
        bool _trace_enabled = false;

    public:
        // Information about the module's ports.
        std::vector<PortBase *> _ports;
        std::vector<InputBase *> _inputs;
        std::vector<OutputBase *> _outputs;

        Module(ModuleName &name);
        Module();

        bool trace_enabled() const { return _trace_enabled; }
        void _end_construction();

        // Opt out of the initial evaluation pass Context::elaborate() otherwise schedules for every module.
        void dont_initialize();
        void _set_initialize(bool value);
        bool initialize() const;
    };

} // namespace dspsim

#define DSPSIM_CTOR(module_name) module_name(::dspsim::ModuleName)