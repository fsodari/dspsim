/*
    Coroutine tasks: what it costs to structure a process as functions that wait, to create processes while
    the simulation runs, and to drive a design from ordinary code. A dspsim coroutine is stackless and a
    SystemC thread has a stack, so these are the features that the two engines implement most differently.

    subtask:   workers in series that repeat a two-cycle transaction, written in the process body
               (inline), as one awaited task (task) and as tasks nested three deep (nested). dspsim
               allocates a coroutine frame per task, SystemC calls a function on the stack of the thread.
    bus:       masters that access a register file through read() and write() tasks built on a
               request/acknowledge handshake (task), and the same master as a state machine (method).
    spawn:     a dispatcher that creates a process per clock cycle, which lives for a few cycles and ends:
               a coroutine or thread (coro), and a function process that asks for its next evaluation
               (method). Creating a process, and what the engine does with one that has ended.
    run_until: the testbench performs bus accesses one by one from ordinary code. dspsim runs the simulation
               until a task completes, SystemC until a spawned thread pauses it. The designs differ in what
               else there is: a single idle process, a thousand of them, or a thousand pending timers.
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/chain.h"
#include "models/chain_sc.h"
#include "models/tasks.h"
#include "models/tasks_sc.h"

#include <cstdint>
#include <string>

using namespace benchmarks;

namespace
{
    // The background timers of the run_until benchmarks expire one after the other at this interval. It is
    // not a multiple of the clock period: they expire on clock edges and between them.
    constexpr uint64_t timer_interval = 97;

    // Add one kind of a design made of `stages` stages in series. Stage and StageSC are the dspsim and
    // SystemC stage.
    template <template <typename> class Stage, template <typename> class StageSC, typename T>
    void add_chain(Suite &suite, const std::string &group, const std::string &kind, int stages, Params params)
    {
        Case &c = suite.add(group, kind, params);
        c.dspsim<ClockedTb<Chain<Stage<T>, T>, T>>(stages);
        c.systemc<ClockedTbSC<ChainSC<StageSC<T>, T>, T>>(stages);
    }

    /*
        Testbench that drives a bus from ordinary code, one access at a time: a step writes a register and
        reads another one back, each with Context::run_until() on the task of the bus master. Nothing else
        in the design does anything on the bus clock, so what a step costs beyond the accesses is the cost
        of run_until(): registering the task as a process, running it, and removing it again.

        The background is `idle_count` flip-flops on a clock that is stopped, which are processes that never
        run, and `timer_count` timers, which are time events that are pending.
    */
    template <typename T>
    class RunUntilTb : public DspsimTb
    {
    public:
        RunUntilTb(int idle_count, int timer_count)
            : idle_chain("idle", idle_count), timers("timers", timer_count, timer_interval)
        {
            bus.clk.bind(clk);
            idle_chain.clk.bind(idle_clk);
            idle_chain.in.bind(idle_in);
            idle_chain.out.bind(idle_out);
        }

        T step(Stimulus &stimulus)
        {
            const T x = stimulus.value<T>();
            ctx->run_until(bus.master.write(x & BusSlave<T>::address_mask, x));
            const T y = ctx->run_until(bus.master.read((x >> 6) & BusSlave<T>::address_mask));
            // The timers that have expired by now are part of the result: the background is checked as well.
            return static_cast<T>(y + timers.expirations);
        }

        dspsim::Clock clk{"clk", clock_period};
        Bus<T> bus{"bus"};

        dspsim::Signal<uint8_t> idle_clk{"idle_clk"};
        dspsim::Signal<T> idle_in{"idle_in"};
        dspsim::Signal<T> idle_out{"idle_out"};
        Chain<DffMethod<T>, T> idle_chain;
        Timers timers;
    };

    /*
        The SystemC counterpart. SystemC has no run_until(): the testbench spawns a thread that performs the
        access and pauses the simulation, and sc_start() returns when the simulation is paused.
    */
    template <typename T>
    class RunUntilTbSC : public SystemCTb
    {
    public:
        RunUntilTbSC(int idle_count, int timer_count)
            : idle_chain("idle", idle_count), timers("timers", timer_count, timer_interval)
        {
            bus.clk(clk);
            idle_chain.clk(idle_clk);
            idle_chain.in(idle_in);
            idle_chain.out(idle_out);
        }

        T step(Stimulus &stimulus)
        {
            const T x = stimulus.value<T>();
            run_until([this, x]
                      { bus.master.write(x & BusSlaveSC<T>::address_mask, x); });
            T y = 0;
            run_until([this, x, &y]
                      { y = bus.master.read((x >> 6) & BusSlaveSC<T>::address_mask); });
            return static_cast<T>(y + timers.expirations);
        }

        sc_core::sc_clock clk{"clk", sc_units(clock_period)};
        // Every access is a thread of its own, so the signals of the master have many writers.
        BusSC<T, sc_core::SC_MANY_WRITERS> bus{"bus"};

        sc_core::sc_signal<bool> idle_clk{"idle_clk"};
        sc_core::sc_signal<T> idle_in{"idle_in"};
        sc_core::sc_signal<T> idle_out{"idle_out"};
        ChainSC<DffMethodSC<T>, T> idle_chain;
        TimersSC timers;

    private:
        // Run the simulation until `transaction`, called in a thread of its own, has returned.
        template <typename F>
        void run_until(F transaction)
        {
            sc_core::sc_spawn([&transaction]
                              {
                                  transaction();
                                  sc_core::sc_pause();
                              });
            sc_core::sc_start();
        }
    };

    void add_run_until(Suite &suite, const std::string &group, int idle, int timers, Params params)
    {
        using T = uint32_t;
        Case &c = suite.add(group, "task", params);
        c.dspsim<RunUntilTb<T>>(idle, timers);
        c.systemc<RunUntilTbSC<T>>(idle, timers);
    }
}

int sc_main(int argc, char *argv[])
{
    Suite suite{"tasks"};
    using T = uint32_t;

    {
        // A value takes two clock cycles per worker: the warm-up has to fill the chain.
        const Params params{.iters = 50'000, .warmup = 1'000};
        add_chain<WorkerInline, WorkerInlineSC, T>(suite, "subtask/n=64", "inline", 64, params);
        add_chain<WorkerTask, WorkerTaskSC, T>(suite, "subtask/n=64", "task", 64, params);
        add_chain<WorkerNested, WorkerNestedSC, T>(suite, "subtask/n=64", "nested", 64, params);
    }
    {
        // 16 buses in series. A stage passes a value on after a write and a read, 7 clock cycles on average.
        const Params params{.iters = 60'000, .warmup = 2'000};
        add_chain<BusMethod, BusMethodSC, T>(suite, "bus/n=16", "method", 16, params);
        add_chain<BusTask, BusTaskSC, T>(suite, "bus/n=16", "task", 16, params);
    }
    {
        // One job per iteration, which lives 1 to 8 clock cycles. SystemC limits the iterations: it takes
        // about 10 us to create and destroy a thread. The peak RSS of a run shows whether an engine keeps
        // the processes that have ended.
        const Params params{.iters = 20'000};
        suite.add("spawn/life=1-8", "method", params)
            .dspsim<ClockedTb<SpawnMethod<T>, T>>()
            .systemc<ClockedTbSC<SpawnMethodSC<T>, T>>();
        suite.add("spawn/life=1-8", "coro", params)
            .dspsim<ClockedTb<SpawnCoro<T>, T>>()
            .systemc<ClockedTbSC<SpawnThreadSC<T>, T>>();
    }
    {
        // An iteration is two accesses, 7 clock cycles on average. SystemC limits the iterations here as
        // well, with two threads per iteration.
        const Params params{.iters = 10'000};
        add_run_until(suite, "run_until/idle=1", 1, 0, params);
        add_run_until(suite, "run_until/idle=1000", 1000, 0, params);
        add_run_until(suite, "run_until/timers=1000", 1, 1000, params);
    }
    return suite.main(argc, argv);
}
