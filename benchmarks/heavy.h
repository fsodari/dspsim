#pragma once
#include <dspsim/dspsim.h>
#include <systemc>
#include "eval_funcs.h"

namespace benchmarks
{
    // Same model with dspsim
    template <typename T, int N>
    class HeavyDff : public dspsim::Module
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        HeavyDff(dspsim::ModuleName name) : dspsim::Module(name)
        {
            context()->register_method(&HeavyDff<T, N>::eval, this, "eval")->always(clk.pos());
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
    class HeavyWire : public dspsim::Module
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        HeavyWire(dspsim::ModuleName name) : dspsim::Module(name)
        {
            context()->register_method(&HeavyWire<T, N>::eval, this, "eval")->always("*");
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
