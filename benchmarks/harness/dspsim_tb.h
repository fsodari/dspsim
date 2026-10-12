/*
    Testbenches for dspsim designs. See bench.h for what a testbench provides.
*/
#pragma once
#include "bench.h"

#include <dspsim/dspsim.h>

#include <cstdint>
#include <memory>

namespace benchmarks
{
    /*
        Base class of a dspsim testbench. It creates the context before the members of the derived class,
        which are the design: signals, clocks and modules.
    */
    class DspsimTb
    {
    public:
        DspsimTb() : ctx(dspsim::Context::create()) {}

        /// Elaborate the design and settle it at time 0, which SystemC does in its first sc_start().
        void start()
        {
            ctx->elaborate();
            ctx->run(0);
        }

        uint64_t time() const { return ctx->time(); }

        std::shared_ptr<dspsim::Context> ctx;
    };

    /*
        Testbench for a module M with a clock, an input and an output of type T (clk, in, out), constructed
        as M("dut", args...). A step writes a random input, runs STEP time units and returns the output.
    */
    template <typename M, typename T, uint64_t STEP = clock_period>
    class ClockedTb : public DspsimTb
    {
    public:
        template <typename... Args>
        explicit ClockedTb(const Args &...args) : dut("dut", args...)
        {
            dut.clk.bind(clk);
            dut.in.bind(in);
            dut.out.bind(out);
        }

        void start()
        {
            DspsimTb::start();
            ctx->run(step_offset);
        }

        T step(Stimulus &stimulus)
        {
            in.write(stimulus.value<T>());
            ctx->run(STEP);
            return out.read();
        }

        dspsim::Clock clk{"clk", clock_period};
        dspsim::Signal<T> in{"in"};
        dspsim::Signal<T> out{"out"};
        M dut;
    };
}
