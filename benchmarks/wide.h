#pragma once
#include <dspsim/dspsim.h>
#include <systemc>
#include <vector>
#include "eval_funcs.h"
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>

#define USE_COROUTINES false

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
#if USE_COROUTINES
            DSPSIM_CORO(eval)
                ->always(clk, in);
#else
            DSPSIM_METHOD(eval)
                ->always(clk, in);
#endif
        }

#if USE_COROUTINES
        dspsim::Task eval()
        {
            while (true)
            {
                T sum = 0;
                for (size_t i = 0; i < N; ++i)
                {
                    sigs[i].write(in.read());
                    sum += sigs[i].read();
                }
                out.write(sum);
                co_await wait();
            }
        }
#else
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
#endif
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
#if USE_COROUTINES
            SC_THREAD(eval);
#else
            SC_METHOD(eval);

#endif
            sensitive << clk << in;
        }
#if USE_COROUTINES
        void eval()
        {
            while (true)
            {
                T sum = 0;
                for (size_t i = 0; i < N; ++i)
                {
                    sigs[i].write(in.read());
                    sum += sigs[i].read();
                }
                out.write(sum);
                wait(); // Wait for the next clock edge
            }
        }
#else
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
#endif
    };
}
