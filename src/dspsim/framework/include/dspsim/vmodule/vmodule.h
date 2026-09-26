#pragma once
#include <dspsim/context.h>
#include <dspsim/module.h>
#include <dspsim/vmodule/vport.h>

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

        void eval()
        {
            // Update the inputs to the verilated model.
            this->_sync_inputs();
            // Evaluate the verilated model.
            top->eval();
            // Update the outputs from the verilated model.
            this->_sync_outputs();
        }

        void _sync_inputs()
        {
            for (auto &input : inputs())
            {
                input->_sync();
            }
        }

        void _sync_outputs()
        {
            for (auto &output : this->outputs())
            {
                output->_sync();
            }
        }

    public:
        // Open a trace file and signal to the context that this model should be traced during the trace step.
        // This should be called after the context's time resolution has been set/finalized.
        void open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0)
        {
            context()->trace_model(this);
            static_cast<Derived *>(this)->_open_trace(trace_path, levels, options);
        }

        // Context will call this at the end of a delta cycle.
        void dump_trace() override
        {
            static_cast<Derived *>(this)->_dump_trace();
        }

        // Close the trace file.
        void close_trace()
        {
            static_cast<Derived *>(this)->_close_trace();
        }

    protected:
        // Default implementations if inherited class does not override them.
        void _open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0)
        {
            this->context()->log("warn", "Tracing not implemented for module: " + this->hier_name());
        }
        void _dump_trace() {}
        void _close_trace() {}
    };
} // namespace dspsim

// Macro to declare a VModule subclass.
#define DSPSIM_VMOD(_name, _vtype) \
    struct _name : public ::dspsim::VModule<_vtype, _name>

// Macro to declare a VModule parameter.
// TODO: pass the underlying model's name, regardless of the prefix. This must be read from the json file so this can only be used in generated code.
#define DSPSIM_VPARAM(_name, _vtype, _param) \
    static constexpr auto _param = _vtype##___024root::_name##__DOT__##_param;
