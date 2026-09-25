#pragma once
#include <dspsim/dspsim.h>
#include <systemc>
#include "eval_funcs.h"
#include <iostream>
#include <spdlog/spdlog.h>
namespace benchmarks
{
    // Same model with dspsim
    template <typename T>
    DSPSIM_MODULE(SimpleDff)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(SimpleDff)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos());
        }

        void eval()
        {
            if (clk.posedge())
            {
                out.write(in.read());
            }
        }
    };

    // Same model with dspsim
    template <typename T>
    DSPSIM_MODULE(SimpleWire)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(SimpleWire)
        {
            DSPSIM_METHOD(eval)
                ->always("*");
        }

        void eval()
        {
            out.write(in.read());
        }
    };

    template <typename T>
    SC_MODULE(SimpleDffSC)
    {
        sc_core::sc_in<bool> clk;
        sc_core::sc_in<T> in;
        sc_core::sc_out<T> out;

        SC_CTOR(SimpleDffSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
        }

        void eval()
        {
            if (clk.posedge())
            {
                out.write(in.read());
            }
        }
    };

    template <typename T>
    SC_MODULE(SimpleWireSC)
    {
        sc_core::sc_in<bool> clk;
        sc_core::sc_in<T> in;
        sc_core::sc_out<T> out;

        SC_CTOR(SimpleWireSC)
        {
            SC_METHOD(eval);
            sensitive << clk << in;
        }

        void eval()
        {
            out.write(in.read());
        }
    };
}
