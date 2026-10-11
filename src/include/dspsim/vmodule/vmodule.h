#pragma once
#include <dspsim/context.h>
#include <dspsim/module.h>
#include <dspsim/vmodule/vport.h>
#include <dspsim/coro.h>

#include <verilated.h>
#include <memory>
#include <filesystem>

namespace dspsim
{
    /*
        Template class for verilated models. Typically this will be used in generated code.
    */
    template <typename V, typename Derived>
    class VModule : public Module
    {
    protected:
        std::unique_ptr<VerilatedContext> vcontext = std::make_unique<VerilatedContext>();
        std::unique_ptr<V> top = std::make_unique<V>(vcontext.get());
        std::shared_ptr<VerilatedTraceBaseC> tracep;

        // This class must be subclassed.
        VModule(ModuleName &name) : Module(name) {}
        VModule() : Module() {}

    public:
        void eval()
        {
            // Update the inputs to the verilated model.
            for (auto &input : inputs())
            {
                input->sync();
            }
            // Evaluate the verilated model.
            top->eval();
            // Update the outputs from the verilated model.
            for (auto &output : this->outputs())
            {
                output->sync();
            }
        }

    public:
        // Open a trace file and signal to the context that this model should be traced during the trace step.
        // This should be called after the context's time resolution has been set/finalized.
        void open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0)
        {
            this->request_trace();
            static_cast<Derived *>(this)->open_trace_impl(trace_path, levels, options);
        }

        // Close the trace file.
        void close_trace()
        {
            static_cast<Derived *>(this)->close_trace_impl();
        }

    protected:
        // Context will call this at the end of a delta cycle.
        void dump_trace() override
        {
            static_cast<Derived *>(this)->dump_trace_impl();
        }

        // Default implementations if inherited class does not override them.
        void open_trace_impl(const std::filesystem::path &trace_path, int levels = 99, int options = 0)
        {
            this->context()->log("warn", "Tracing not implemented for module: " + this->hier_name());
        }
        void dump_trace_impl() {}
        void close_trace_impl() {}
    };
} // namespace dspsim

// Macro to declare a VModule subclass.
#define DSPSIM_VMOD(name, vtype) \
    struct name : public ::dspsim::VModule<vtype, name>

// Macro to declare a VModule parameter.
// TODO: pass the underlying model's name, regardless of the prefix. This must be read from the json file so this can only be used in generated code.
#define DSPSIM_VPARAM(name, vtype, param) \
    static constexpr auto param = vtype##___024root::name##__DOT__##param;

// Constructor macro for a module subclass. Uses ModuleName as the constructor argument. A string can be passed as an argument.
#define DSPSIM_CTOR_REF(module_name) module_name(::dspsim::ModuleName &)