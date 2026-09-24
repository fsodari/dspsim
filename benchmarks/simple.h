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
    class SimpleDff : public dspsim::Module
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        SimpleDff(dspsim::ModuleName name) : dspsim::Module(name)
        {
            DSPSIM_METHOD(&SimpleDff<T>::eval)
                ->always(clk);
            // ->always(clk.pos());
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
    class SimpleWire : public dspsim::Module
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        SimpleWire(dspsim::ModuleName name) : dspsim::Module(name)
        {
            DSPSIM_METHOD(&SimpleWire<T>::eval)->always("*");
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
            // sensitive << clk.pos();
            sensitive << clk;
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
