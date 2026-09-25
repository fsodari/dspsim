#pragma once
#include <dspsim/dspsim.h>
#include <systemc>
#include "eval_funcs.h"

namespace benchmarks
{
    // Same model with dspsim
    template <typename T, int N>
    DSPSIM_MODULE(HeavyDff)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(HeavyDff)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos());
        }

        void eval()
        {
            if (clk.posedge())
            {
                out.write(heavy_compute(in.read(), N));
            }
        }
    };

    // Same model with dspsim
    template <typename T, int N>
    DSPSIM_MODULE(HeavyWire)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(HeavyWire)
        {
            DSPSIM_METHOD(eval)
                ->always("*");
        }

        void eval()
        {
            out.write(heavy_compute(in.read(), N));
        }
    };

    template <typename T, int N>
    SC_MODULE(HeavyDffSC)
    {
        sc_core::sc_in<bool> clk;
        sc_core::sc_in<T> in;
        sc_core::sc_out<T> out;

        SC_CTOR(HeavyDffSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
        }

        void eval()
        {
            if (clk.posedge())
            {
                out.write(heavy_compute(in.read(), N));
            }
        }
    };

    template <typename T, int N>
    SC_MODULE(HeavyWireSC)
    {
        sc_core::sc_in<bool> clk;
        sc_core::sc_in<T> in;
        sc_core::sc_out<T> out;

        SC_CTOR(HeavyWireSC)
        {
            SC_METHOD(eval);
            sensitive << clk << in;
        }

        void eval()
        {
            out.write(heavy_compute(in.read(), N));
        }
    };
}
