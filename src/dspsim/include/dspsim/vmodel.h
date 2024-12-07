#pragma once
#include "dspsim/model.h"
#include "dspsim/signal.h"
// #include "dspsim/axis.h"
#include <verilated.h>

namespace dspsim
{
    template <typename V, typename TraceType = VerilatedVcdC>
    class VModel : public Model
    {
    protected:
        std::unique_ptr<VerilatedContext> vcontext;
        std::unique_ptr<V> top;
        // V top;
        std::unique_ptr<TraceType> tfp;

    public:
        VModel() : Model()
        {
            vcontext = std::make_unique<VerilatedContext>();
            top = std::make_unique<V>(vcontext.get());
        }

        void eval_step()
        {
            if (tfp)
            {
                tfp->dump(context()->time());
            }
            top->eval_step();
            // if (tfp)
            // {
            //     tfp->dump(context()->time());
            // }
            // top.eval_step();
        }

        void eval_end_step()
        {
            top->eval_end_step();
            // top.eval_end_step();
            // if (tfp)
            // {
            //     tfp->dump(context()->time());
            // }
        }

        void trace(const std::string &trace_path, int levels = 99, int options = 0)
        {
            // Verilated::traceEverOn(true);
            vcontext->traceEverOn(true);

            tfp = std::make_unique<TraceType>();
            tfp->set_time_resolution("ns");
            tfp->set_time_unit("ns");

            top->trace(tfp.get(), levels, options);
            tfp->open(trace_path.c_str());
        }

        void close()
        {
            tfp->close();
        }
    };
} // namespace dspsim
