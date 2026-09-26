#pragma once
#include <dspsim/dspsim.h>
#include <systemc>
#include <vector>
#include "eval_funcs.h"
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>

namespace benchmarks
{
    template <typename T, int N>
    DSPSIM_MODULE(Wide)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        std::vector<dspsim::Signal<T>> sigs{N};

        DSPSIM_CTOR(Wide)
        {
            DSPSIM_METHOD(eval)
                ->always(clk, in);
        }

        void eval()
        {
            T sum = 0;
            for (size_t i = 0; i < N; ++i)
            {
                sigs[i].write(in.read());
                sum += sigs[i].read();
            }
            out.write(sum);
        }
    };

    template <typename T, int N>
    SC_MODULE(WideSC)
    {
    public:
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        sc_core::sc_vector<sc_core::sc_signal<T>> sigs{"sigs", N};

        SC_CTOR(WideSC)
        {
            SC_METHOD(eval);
            sensitive << clk << in;
        }

        void eval()
        {
            T sum = 0;
            for (size_t i = 0; i < N; ++i)
            {
                sigs[i].write(in.read());
                sum += sigs[i].read();
            }
            out.write(sum);
        }
    };
}
