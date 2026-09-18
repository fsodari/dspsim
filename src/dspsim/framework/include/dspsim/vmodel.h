#pragma once
#include <dspsim/module.h>
#include <verilated.h>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

namespace dspsim
{
    class NoTrace;

    template <typename V, typename TraceType = NoTrace>
    class VModel : public Module
    {
    protected:
        std::unique_ptr<VerilatedContext> vcontext;
        std::unique_ptr<V> top;
        std::unique_ptr<TraceType> tracep;

    public:
        VModel(ModuleName name) : Module(name)
        {
            vcontext = std::make_unique<VerilatedContext>();
            top = std::make_unique<V>(vcontext.get());
        }
        ~VModel()
        {
            close();
        }

        void eval() override
        {
            top->eval_step();
        }

        void update() override
        {
            top->eval_end_step();
            if (tracep)
            {
                tracep->dump(context()->time());
            }
        }

        void trace(const fs::path &trace_path, int levels = 99, int options = 0)
        {
            if (!tracep)
            {
                vcontext->traceEverOn(true);
                tracep = std::make_unique<TraceType>();

                tracep->set_time_unit(context()->time_unit().c_str());
                tracep->set_time_resolution(context()->time_precision().c_str());

                top->trace(tracep.get(), levels, options);
                tracep->open(trace_path.string().c_str());
            }
        }

        void close()
        {
            if (tracep)
            {
                tracep->close();
            }
        }
    };

    /*
        Specialization for no tracing.
    */
    template <typename V>
    class VModel<V, NoTrace> : public Module
    {
    protected:
        std::unique_ptr<VerilatedContext> vcontext;
        std::unique_ptr<V> top;

    public:
        VModel(ModuleName name) : Module(name)
        {
            vcontext = std::make_unique<VerilatedContext>();
            top = std::make_unique<V>(vcontext.get());
        }

        void eval_step() override
        {
            top->eval_step();
        }
        void eval_end_step() override
        {
            top->eval_end_step();
        }
    };

} // namespace dspsim
