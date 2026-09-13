#pragma once
#include <dspsim/model.h>
#include <verilated.h>
// #include <verilated_vcd_c.h>
// #include <verilated_fst_c.h>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

namespace dspsim
{
    class NoTrace;

    template <typename V, typename TraceType = NoTrace>
    class VModel : public Model
    {
    protected:
        std::unique_ptr<VerilatedContext> vcontext;
        std::unique_ptr<V> top;
        std::unique_ptr<TraceType> tracep;

    public:
        VModel(const std::string &name = "") : Model("vmodel", name)
        {
            vcontext = std::make_unique<VerilatedContext>();
            top = std::make_unique<V>(vcontext.get());
        }
        ~VModel()
        {
            close();
        }

        void eval_step() override
        {
            if (tracep)
            {
                tracep->dump(context()->time());
            }
            top->eval_step();
        }

        void eval_end_step() override
        {
            top->eval_end_step();
        }

        void trace(const fs::path &trace_path, int levels = 99, int options = 0)
        {
            if (!tracep)
            {
                // vcontext->timeunit(context()->time_unit().c_str());
                // vcontext->timeprecision(context()->time_precision().c_str());
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
    class VModel<V, NoTrace> : public Model
    {
    protected:
        std::unique_ptr<VerilatedContext> vcontext;
        std::unique_ptr<V> top;

    public:
        VModel(const std::string &name = "") : Model("vmodel", name)
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

        // void trace(const fs::path &trace_path, int levels = 99, int options = 0)
        // {
        //     // No tracing available for this specialization.
        // }
        // void close()
        // {
        //     // No tracing available for this specialization.
        // }
    };

} // namespace dspsim
