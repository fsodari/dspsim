#pragma once
#include <dspsim/model.h>
#include <dspsim/module_name.h>
#include <dspsim/port.h>
#include <vector>

namespace dspsim
{
    class ProcessBase;
    class Module : public Model
    {
        // Information about the module's ports.
        std::vector<PortBase *> _ports;
        std::vector<PortBase *> _inputs;
        std::vector<PortBase *> _outputs;

    public:
        // A subclass of Module should use ModuleName with no reference so that the ModuleName goes out of scope at the end of the subclass constructor.
        Module(ModuleName &name);
        // If called with no arguments, the module will obtain its name from the active module name stack in the context.
        Module();

        // Gets called automatically after the subclass constructor finishes.
        // Removes the module from the active module stack in the context.
        void _end_construction();

        // Schedule a process to be evaluated after the given time delta relative to the current simulation time.
        // If this is ever used multithreaded, process must be explicitly provided since active_process may not be reliable.
        void next_trigger(uint64_t time_delta, ProcessBase *process = nullptr);

        // Schedule to be sensitive to an event on the next trigger.
        void next_trigger(SensitivityEvent &event, ProcessBase *process = nullptr);

        // "*" will be sensitive to all events in the static sensitivity list. Only supported named event for now.
        void next_trigger(const std::string &event_name, ProcessBase *process = nullptr);

        // Coroutine awaitables
        WaitTimeEvent wait(uint64_t time_delta, ProcessBase *process = nullptr);
        WaitSensitivityEvent wait(ProcessBase *process = nullptr);
        WaitSensitivityEvent wait(SensitivityEvent &event, ProcessBase *process = nullptr);
        WaitSensitivityEvent wait(std::initializer_list<std::reference_wrapper<SensitivityEvent>> events, ProcessBase *process = nullptr);

        // Information about a module's ports.
        std::vector<PortBase *> &ports() { return _ports; }
        std::vector<PortBase *> &inputs() { return _inputs; }
        std::vector<PortBase *> &outputs() { return _outputs; }
    };

    template <typename M>
    static inline auto _create_module(ModuleName &name)
    {
        return Model::create<M>(name);
    }

} // namespace dspsim

// Declare a module class that inherits from dspsim::Module.
#define DSPSIM_MODULE(...) \
    struct __VA_ARGS__ : public ::dspsim::Module

// Constructor macro for a module subclass. Uses ModuleName as the constructor argument. A string can be passed as an argument.
#define DSPSIM_CTOR(module_name) module_name(::dspsim::ModuleName)
