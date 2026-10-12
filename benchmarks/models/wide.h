/*
    dspsim model for the wide benchmarks: one process that writes many signals. wide_sc.h has the SystemC
    counterpart.
*/
#pragma once
#include <dspsim/dspsim.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace benchmarks
{
    /*
        Copies the input to N internal signals and outputs the sum of their values. The process runs on
        every clock edge and input change, so an input change makes it write N signals, and the update
        phase commits them: many signal updates in a delta cycle with a single evaluation.
    */
    /// Method process.
    template <typename T, int N>
    DSPSIM_MODULE(WideMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        std::vector<dspsim::Signal<T>> sigs{N};

        DSPSIM_CTOR(WideMethod)
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

    /// Coroutine process with static sensitivity.
    template <typename T, int N>
    DSPSIM_MODULE(WideCoro)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        std::vector<dspsim::Signal<T>> sigs{N};

        DSPSIM_CTOR(WideCoro)
        {
            DSPSIM_CORO(eval)
                ->always(clk, in);
        }

        dspsim::Task<> eval()
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
    };
}
