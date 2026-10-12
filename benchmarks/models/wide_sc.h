/*
    SystemC counterparts of wide.h.
*/
#pragma once
#include <systemc>

#include <cstddef>

namespace benchmarks
{
    /// SC_METHOD.
    template <typename T, int N>
    SC_MODULE(WideMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        sc_core::sc_vector<sc_core::sc_signal<T>> sigs{"sigs", N};

        SC_CTOR(WideMethodSC)
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

    /// SC_THREAD with static sensitivity.
    template <typename T, int N>
    SC_MODULE(WideThreadSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        sc_core::sc_vector<sc_core::sc_signal<T>> sigs{"sigs", N};

        SC_CTOR(WideThreadSC)
        {
            SC_THREAD(eval);
            sensitive << clk << in;
        }

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
                wait();
            }
        }
    };
}
