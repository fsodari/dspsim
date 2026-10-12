/*
    Testbenches for SystemC designs, the counterparts of dspsim_tb.h. One time unit is 1 ns.
*/
#pragma once
#include "bench.h"

#include <systemc>

#include <cstdint>

namespace benchmarks
{
    /// A time in the time units of the benchmarks.
    inline sc_core::sc_time sc_units(uint64_t time)
    {
        return sc_core::sc_time(static_cast<double>(time), sc_core::SC_NS);
    }

    /// Base class of a SystemC testbench.
    class SystemCTb
    {
    public:
        /*
            Elaborate the design and settle it at time 0. sc_start(SC_ZERO_TIME) runs a single delta cycle,
            so repeat it until nothing is left to do at the current time, which is what dspsim's run(0) does.
        */
        void start()
        {
            sc_core::sc_start(sc_core::SC_ZERO_TIME);
            while (sc_core::sc_pending_activity_at_current_time())
            {
                sc_core::sc_start(sc_core::SC_ZERO_TIME);
            }
        }

        uint64_t time() const
        {
            return sc_core::sc_time_stamp().value() / sc_units(1).value();
        }
    };

    /// The counterpart of ClockedTb, for an sc_module M with the ports clk, in and out.
    template <typename M, typename T, uint64_t STEP = clock_period>
    class ClockedTbSC : public SystemCTb
    {
    public:
        template <typename... Args>
        explicit ClockedTbSC(const Args &...args) : dut("dut", args...)
        {
            dut.clk(clk);
            dut.in(in);
            dut.out(out);
        }

        void start()
        {
            SystemCTb::start();
            sc_core::sc_start(sc_units(step_offset));
        }

        T step(Stimulus &stimulus)
        {
            in.write(stimulus.value<T>());
            sc_core::sc_start(step_);
            return out.read();
        }

        sc_core::sc_clock clk{"clk", sc_units(clock_period)};
        sc_core::sc_signal<T> in{"in"};
        sc_core::sc_signal<T> out{"out"};
        M dut;

    private:
        // Converting a number to an sc_time costs a floating point multiplication and a rounding: do it once.
        const sc_core::sc_time step_ = sc_units(STEP);
    };
}
