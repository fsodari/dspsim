#pragma once
#include <dspsim/context.h>
#include <dspsim/vmodule/vmodule_base.h>
#include <dspsim/vmodule/vport.h>
#include <verilated.h>
#include <memory>
#include <filesystem>

#define DSPSIM_VMOD(_name, _vtype) \
    class _name : public ::dspsim::VModule<_vtype, _name>

#define DSPSIM_VCTOR_PART(_name, _vtype) \
    _name(::dspsim::ModuleName name) : ::dspsim::VModule<_vtype, _name>(name)

#define DSPSIM_VCTOR(_name, _vtype)                                                           \
    _name(::dspsim::ModuleName name) : ::dspsim::VModule<_vtype, _name>(name)                 \
    {                                                                                         \
        context()->register_method(&_name::eval_top, this, #_name "::eval_top")->always("*"); \
    }

#define DSPSIM_VPARAM(_name, _vtype, _param) \
    static constexpr auto _param = _vtype##___024root::_name##__DOT__##_param;

namespace dspsim
{
    // template <typename V>
    template <typename V, typename Derived>
    class VModule : public VModuleBase
    {
    public:
        std::unique_ptr<VerilatedContext> vcontext = std::make_unique<VerilatedContext>();
        std::unique_ptr<V> top = std::make_unique<V>(vcontext.get());
        std::shared_ptr<VerilatedTraceBaseC> tracep;

        VModule(ModuleName &name) : VModuleBase(name)
        {
        }

        void eval_top()
        {
            this->_sync_inputs();
            top->eval();
            this->_sync_outputs();
        }

        void dump_trace() override
        {
            static_cast<Derived *>(this)->_dump_trace();
        }

        void open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0)
        {
            context()->trace_model(this);
            static_cast<Derived *>(this)->_open_trace(trace_path, levels, options);
        }

        void close_trace()
        {
            static_cast<Derived *>(this)->_close_trace();
        }

        // Default implementations if inherited class does not override them.
        void _dump_trace() {}
        void _open_trace(const std::filesystem::path &trace_path, int levels = 99, int options = 0)
        {
            this->context()->log("warn", "Tracing not implemented for module: " + this->hier_name());
        }
        void _close_trace() {}
    };
} // namespace dspsim
