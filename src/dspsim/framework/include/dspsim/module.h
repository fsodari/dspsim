#pragma once
#include <dspsim/model.h>
#include <dspsim/module_name.h>
#include <dspsim/port.h>
#include <vector>

namespace dspsim
{
    class Module : public Model
    {
        // Information about the module's ports.
        std::vector<PortBase *> _ports;
        std::vector<InputBase *> _inputs;
        std::vector<OutputBase *> _outputs;

    public:
        // A subclass of Module should use ModuleName with no reference so that the ModuleName goes out of scope at the end of the subclass constructor.
        Module(ModuleName &name);
        // If called with no arguments, the module will obtain its name from the active module name stack in the context.
        Module();

        // Gets called automatically after the subclass constructor finishes.
        // Removes the module from the active module stack in the context.
        void _end_construction();

        // Information about a module's ports.
        std::vector<PortBase *> &ports() { return _ports; }
        std::vector<InputBase *> &inputs() { return _inputs; }
        std::vector<OutputBase *> &outputs() { return _outputs; }
    };

} // namespace dspsim

// Declare a module class that inherits from dspsim::Module.
#define DSPSIM_MODULE(...) struct __VA_ARGS__ : public ::dspsim::Module

// Constructor macro for a module subclass. Uses ModuleName as the constructor argument. A string can be passed as an argument.
#define DSPSIM_CTOR(module_name) module_name(::dspsim::ModuleName)
