#pragma once
#include <dspsim/model.h>
#include <dspsim/module_name.h>
// A module registers its processes with DSPSIM_METHOD / DSPSIM_CORO, which need the complete Context and
// ProcessBase, and its coroutines await the awaitables returned by wait().
#include <dspsim/context.h>
#include <dspsim/coro.h>
#include <dspsim/process.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace dspsim
{
    class PortBase;
    class SensitivityEvent;
    class Module : public Model
    {
        // Ports register with the module they are declared in.
        friend class PortBase;
        // The ModuleName scope ends the module's construction.
        friend class ModuleName;

    public:
        // A subclass of Module should use ModuleName with no reference so that the ModuleName goes out of scope at the end of the subclass constructor.
        Module(ModuleName &name);
        // If called with no arguments, the module will obtain its name from the active module name stack in the context.
        Module();

        // next_trigger() is for method processes. In a coroutine process it throws std::logic_error: use wait().
        // Schedule a process to be evaluated after the given time delta relative to the current simulation time.
        // If this is ever used multithreaded, process must be explicitly provided since active_process may not be reliable.
        void next_trigger(uint64_t time_delta);

        // Schedule to be sensitive to an event on the next trigger.
        void next_trigger(SensitivityEvent &event);
        // Schedule to be sensitive to whichever of the events occurs first, like several next_trigger(event) calls.
        void next_trigger(const std::vector<std::reference_wrapper<SensitivityEvent>> &events);
        void next_trigger(const std::vector<SensitivityEvent *> &events);

        // "*" will be sensitive to all events in the static sensitivity list. Only supported named event for now.
        void next_trigger(const std::string &event_name);

        // Coroutine awaitables. See Context::wait().
        // wait() is defined inline: a coroutine with static sensitivity calls it on every evaluation.
        Wait wait() { return Wait{context()->current_process()}; }
        WaitTimeEvent wait(uint64_t time_delta);
        WaitSensitivityEvent wait(SensitivityEvent &event);
        WaitSensitivityEvent wait(const std::vector<std::reference_wrapper<SensitivityEvent>> &events);
        WaitSensitivityEvent wait(const std::vector<SensitivityEvent *> &events);
        // With a timeout (> 0): the result tells whether an event triggered or the timeout elapsed.
        WaitSensitivityEvent wait(SensitivityEvent &event, uint64_t timeout);
        WaitSensitivityEvent wait(const std::vector<std::reference_wrapper<SensitivityEvent>> &events, uint64_t timeout);
        WaitSensitivityEvent wait(const std::vector<SensitivityEvent *> &events, uint64_t timeout);

        // The ports declared in this module, in declaration order.
        // Defined inline: a verilated module walks its inputs and outputs on every evaluation.
        const std::vector<PortBase *> &ports() const { return ports_; }
        const std::vector<PortBase *> &inputs() const { return inputs_; }
        const std::vector<PortBase *> &outputs() const { return outputs_; }

    private:
        // Name of the innermost ModuleName scope, for modules constructed without a ModuleName argument.
        static std::string current_module_name();
        // The running process, for next_trigger(event): a coroutine process must wait(event) instead.
        static ProcessBase *method_process(Context *context);

        // Called by the ModuleName scope once the most derived constructor has finished.
        // Removes the module from the active module stack in the context.
        void end_construction();
        // Called by a port constructed in this module. The port's kind says if it is an input or an output.
        void add_port(PortBase *port);

    private:
        // Information about the module's ports.
        std::vector<PortBase *> ports_;
        std::vector<PortBase *> inputs_;
        std::vector<PortBase *> outputs_;
    };

    /// Create a module that its context shares the ownership of. `Args` are the types of the constructor
    /// arguments after the name. They are not deduced: bindings take the address of an instance,
    /// nanobind::new_(&create_module<M, int, const std::string &>).
    template <typename M, typename... Args>
    inline auto create_module(ModuleName &name, Args... args)
    {
        return Model::create<M>(name, std::forward<Args>(args)...);
    }

} // namespace dspsim

// Declare a module class that inherits from dspsim::Module.
#define DSPSIM_MODULE(...) \
    struct __VA_ARGS__ : public ::dspsim::Module

// Constructor macro for a module subclass. Uses ModuleName as the constructor argument. A string can be passed as an argument.
#define DSPSIM_CTOR(module_name) module_name(::dspsim::ModuleName)
