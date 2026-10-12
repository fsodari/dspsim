/*
    Simulated time: designs in which the processes wait for different times, so that the engine's
    time-ordered event queue holds many entries and time advances in many small steps.

    timers:  processes that wake up periodically, each with its own period. The queue holds one entry per
             timer, and every wakeup takes one out and puts one in.
    clocks:  clock domains, each with a clock of its own period and a counter on it.
    timeout: bus masters that wait for the answer of their slave with a timeout. `ack`: the answer always
             arrives long before the timeout, which leaves an unused timeout in the queue for every
             transfer. `expire`: the answer always comes too late.
    delay:   a delay line. A change of the input travels through it, one stage and one time step at a time.

    Each design is a chain of stages (see models/timing.h), so that its output depends on all of them.
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/timing.h"
#include "models/timing_sc.h"

#include <string>
#include <vector>

using namespace benchmarks;

namespace
{
    /*
        The designs of this suite keep time themselves: they have an input and an output, but no clock. The
        testbenches below are ClockedTb and ClockedTbSC without the clock.

        Both engines leave what is due exactly at the end of a run for the next run, and the testbench
        changes the input there. To keep the designs away from that point, a step ends at an odd time, and
        the timers, clocks and bus transfers only act at even times: their periods, gaps, latencies and
        timeouts are even, and a clock period is a multiple of 4, because a clock also acts after half a
        period. The delay line acts when its input has changed, see stage_delays().
    */
    constexpr uint64_t sample_offset = 1;

    /// Testbench for a module M with an input and an output of type T (in, out), constructed as M("dut", args...).
    template <typename M, typename T, uint64_t STEP>
    class TimedTb : public DspsimTb
    {
    public:
        template <typename... Args>
        explicit TimedTb(const Args &...args) : dut("dut", args...)
        {
            dut.in.bind(in);
            dut.out.bind(out);
        }

        void start()
        {
            DspsimTb::start();
            ctx->run(sample_offset);
        }

        T step(Stimulus &stimulus)
        {
            in.write(stimulus.value<T>());
            ctx->run(STEP);
            return out.read();
        }

        dspsim::Signal<T> in{"in"};
        dspsim::Signal<T> out{"out"};
        M dut;
    };

    /// The counterpart of TimedTb, for an sc_module M with the ports in and out.
    template <typename M, typename T, uint64_t STEP>
    class TimedTbSC : public SystemCTb
    {
    public:
        template <typename... Args>
        explicit TimedTbSC(const Args &...args) : dut("dut", args...)
        {
            dut.in(in);
            dut.out(out);
        }

        void start()
        {
            SystemCTb::start();
            sc_core::sc_start(sc_units(sample_offset));
        }

        T step(Stimulus &stimulus)
        {
            in.write(stimulus.value<T>());
            sc_core::sc_start(step_);
            return out.read();
        }

        sc_core::sc_signal<T> in{"in"};
        sc_core::sc_signal<T> out{"out"};
        M dut;

    private:
        const sc_core::sc_time step_ = sc_units(STEP);
    };

    // Add one kind of a chain: Stage and StageSC are the dspsim and SystemC stage, constructed with one
    // element of `times` each and with `args`. A step runs STEP time units.
    template <typename Stage, typename StageSC, typename T, uint64_t STEP, typename... Args>
    void add_chain(Suite &suite, const std::string &group, const std::string &kind, Params params,
                   const std::string &description, const std::vector<uint64_t> &times, Args... args)
    {
        Case &c = suite.add(group, kind, params, description);
        c.dspsim<TimedTb<TimedChain<Stage, T>, T, STEP>>(times, args...);
        c.systemc<TimedTbSC<TimedChainSC<StageSC, T>, T, STEP>>(times, args...);
    }

    // An even time in [low, high).
    uint64_t even_time(Stimulus &random, uint64_t low, uint64_t high)
    {
        return 2 * (low / 2 + random.below(static_cast<uint32_t>((high - low) / 2)));
    }

    /*
        Periods of n timers. Every other timer has one of a few common periods, and the timers with the same
        period always wake up together. The others have a period of their own, and wake up together with
        another timer now and then.
    */
    std::vector<uint64_t> timer_periods(size_t n)
    {
        constexpr uint64_t common[] = {200, 500, 800, 1200};
        Stimulus random{1};
        std::vector<uint64_t> periods;
        for (size_t i = 0; i < n; ++i)
        {
            periods.push_back(i % 2 == 0 ? common[(i / 2) % 4] : even_time(random, 100, 2000));
        }
        return periods;
    }

    /// Periods of n clocks, all different: 8, 12, 16, ...
    std::vector<uint64_t> clock_periods(size_t n)
    {
        std::vector<uint64_t> periods;
        for (size_t i = 0; i < n; ++i)
        {
            periods.push_back(4 * (i + 2));
        }
        return periods;
    }

    /// Idle times of n bus masters between two transfers, in [10, 50).
    std::vector<uint64_t> master_gaps(size_t n)
    {
        Stimulus random{2};
        std::vector<uint64_t> gaps;
        for (size_t i = 0; i < n; ++i)
        {
            gaps.push_back(even_time(random, 10, 50));
        }
        return gaps;
    }

    /*
        Delays of the n stages of a delay line whose input changes every `step` time units, in [10, 200].
        The output of a stage changes when the delays up to that stage have passed since the input changed.
        That must not be a multiple of the step, or the stage would act exactly at the end of a later step.
    */
    std::vector<uint64_t> stage_delays(size_t n, uint64_t step)
    {
        Stimulus random{3};
        std::vector<uint64_t> delays;
        uint64_t total = 0;
        for (size_t i = 0; i < n; ++i)
        {
            uint64_t delay = 10 + random.below(190);
            if ((total + delay) % step == 0)
            {
                ++delay;
            }
            total += delay;
            delays.push_back(delay);
        }
        return delays;
    }
}

int sc_main(int argc, char *argv[])
{
    // The SystemC masters ask timed_out() why a wait ended, which SystemC reports as deprecated, once.
    sc_core::sc_report_handler::set_actions(sc_core::SC_ID_IEEE_1666_DEPRECATION_, sc_core::SC_DO_NOTHING);

    Suite suite{"timing"};
    using T = uint32_t;

    {
        // About 24 wakeups in a step, at 11 distinct times. The warm-up is longer than a change of the input
        // needs to reach the output: every timer on the way has to wake up once.
        constexpr uint64_t step = 1000;
        const Params params{.iters = 80'000, .warmup = 100};
        const auto periods = timer_periods(16);
        add_chain<TimerMethod<T>, TimerMethodSC<T>, T, step>(
            suite, "timers/n=16", "method_dynamic", params, "next_trigger(period)", periods);
        add_chain<TimerCoro<T>, TimerThreadSC<T>, T, step>(
            suite, "timers/n=16", "coro_dynamic", params, "wait(period) in a loop", periods);
    }
    {
        // The same with 64 times the timers: about 2000 wakeups in a step, at 240 distinct times, and a
        // queue of 1024 entries.
        constexpr uint64_t step = 1000;
        const Params params{.iters = 600, .warmup = 1000};
        const auto periods = timer_periods(1024);
        add_chain<TimerMethod<T>, TimerMethodSC<T>, T, step>(
            suite, "timers/n=1024", "method_dynamic", params, "next_trigger(period)", periods);
        add_chain<TimerCoro<T>, TimerThreadSC<T>, T, step>(
            suite, "timers/n=1024", "coro_dynamic", params, "wait(period) in a loop", periods);
    }
    {
        // 16 clocks with periods from 8 to 68: about 60 rising and 60 falling edges in a step.
        constexpr uint64_t step = 100;
        const Params params{.iters = 10'000, .warmup = 100};
        const auto periods = clock_periods(16);
        add_chain<DomainMethod<T>, DomainMethodSC<T>, T, step>(
            suite, "clocks/n=16", "method", params, "a clock and a method process per domain", periods);
        add_chain<DomainCoro<T>, DomainThreadSC<T>, T, step>(
            suite, "clocks/n=16", "coro", params, "a clock and a coroutine per domain", periods);
    }
    {
        // The slave answers after 6 time units and the master would wait for 1000. There are about 200
        // transfers in a step, so about 2000 unused timeouts are pending at any time, 32 per master.
        constexpr uint64_t step = 100;
        constexpr uint64_t latency = 6;
        constexpr uint64_t timeout = 1000;
        const Params params{.iters = 1'500, .warmup = 200};
        const auto gaps = master_gaps(64);
        add_chain<BusPair<MasterEvent<T>, T>, BusPairSC<MasterEventSC<T>, T>, T, step>(
            suite, "timeout/ack,n=64", "coro_dynamic", params, "wait(ack), no timeout",
            gaps, latency, timeout);
        add_chain<BusPair<MasterTimeout<T>, T>, BusPairSC<MasterTimeoutSC<T>, T>, T, step>(
            suite, "timeout/ack,n=64", "coro_timeout", params, "wait(ack, timeout), the ack arrives",
            gaps, latency, timeout);
    }
    {
        // The master waits for 20 time units and the slave answers after 26, which is during the gap that
        // follows (10 or more): the master does not see it, and the slave is ready for the next request.
        // About 140 transfers in a step.
        constexpr uint64_t step = 100;
        constexpr uint64_t latency = 26;
        constexpr uint64_t timeout = 20;
        const Params params{.iters = 3'000, .warmup = 200};
        const auto gaps = master_gaps(64);
        add_chain<BusPair<MasterTime<T>, T>, BusPairSC<MasterTimeSC<T>, T>, T, step>(
            suite, "timeout/expire,n=64", "coro_dynamic", params, "wait(timeout), no event",
            gaps, latency, timeout);
        add_chain<BusPair<MasterTimeout<T>, T>, BusPairSC<MasterTimeoutSC<T>, T>, T, step>(
            suite, "timeout/expire,n=64", "coro_timeout", params, "wait(ack, timeout), the timeout expires",
            gaps, latency, timeout);
    }
    {
        // The input changes every 1000 time units and takes about 10600 to reach the output, so 10 or 11
        // changes are on their way, each at a different stage. Every stage acts once in a step, nearly
        // always at a time of its own. The warm-up lets the first changes arrive.
        constexpr uint64_t step = 1000;
        const Params params{.iters = 10'000, .warmup = 100};
        const auto delays = stage_delays(100, step);
        add_chain<DelayMethod<T>, DelayMethodSC<T>, T, step>(
            suite, "delay/n=100", "method_dynamic", params, "next_trigger(in), next_trigger(delay)", delays);
        add_chain<DelayCoro<T>, DelayThreadSC<T>, T, step>(
            suite, "delay/n=100", "coro_dynamic", params, "wait(in), wait(delay)", delays);
    }
    return suite.main(argc, argv);
}
