/*
    dspsim models for the timing benchmarks: stages that keep time in different ways (a periodic timer, a
    clock domain, a bus transfer with a timeout, a propagation delay), and TimedChain, which connects stages
    in series. timing_sc.h has the SystemC counterparts.

    Every stage has an input and an output and passes on what it reads, changed by the work it did, so the
    output of a chain depends on every stage and on the time at which each one ran. A stage reads the value
    its input had before the current delta cycle, so the order in which the engine evaluates the stages that
    run at the same time does not matter.
*/
#pragma once
#include "level_storage.h"

#include <dspsim/dspsim.h>

#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <vector>

namespace benchmarks
{
    /*
        A periodic timer: a process that wakes up every `period` time units, as a model does that polls a
        status line, samples a bus or generates a tick. Every wakeup is an entry in the engine's time queue,
        so N timers keep N entries in it, and each wakeup takes one out and puts one in.
    */
    /// Method process without static sensitivity: next_trigger(time) on every evaluation.
    template <typename T>
    DSPSIM_MODULE(TimerMethod)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        TimerMethod(::dspsim::ModuleName, uint64_t period) : period_(period)
        {
            DSPSIM_METHOD(eval);
        }

        void eval()
        {
            // The count makes every wakeup change the output, whether the input has changed or not.
            out.write(in.read() + ++count_);
            next_trigger(period_);
        }

    private:
        uint64_t period_;
        T count_ = 0;
    };

    /// Coroutine process that waits for the time.
    template <typename T>
    DSPSIM_MODULE(TimerCoro)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        TimerCoro(::dspsim::ModuleName, uint64_t period) : period_(period)
        {
            DSPSIM_CORO(eval);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                out.write(in.read() + ++count_);
                co_await wait(period_);
            }
        }

    private:
        uint64_t period_;
        T count_ = 0;
    };

    /*
        A clock domain: a clock of its own and a counter on its rising edges, which adds its count to the
        value it samples from the domain before it. N domains are N clocks with different periods, each a
        process of the engine that reschedules itself twice per period and a signal with one subscriber.
    */
    /// Method process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(DomainMethod)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Clock clk;

        DomainMethod(::dspsim::ModuleName, uint64_t period) : clk("clk", period)
        {
            // The coroutine kind only counts clock edges, so skip the initial evaluation.
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            out.write(in.read() + ++count_);
        }

    private:
        T count_ = 0;
    };

    /// Coroutine process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(DomainCoro)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Clock clk;

        DomainCoro(::dspsim::ModuleName, uint64_t period) : clk("clk", period)
        {
            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait();
                out.write(in.read() + ++count_);
            }
        }

    private:
        T count_ = 0;
    };

    /*
        A bus master and its slave, as in a transaction-level model of a bus. The master starts a transfer by
        changing `req`, waits for the slave to change `ack`, and idles for `gap` time units before the next
        transfer. The slave answers a request after `latency` time units. The masters differ in how long
        they are prepared to wait:

            MasterTimeout   for the answer, but no longer than `timeout`: wait(event, timeout)
            MasterEvent     for the answer, however long it takes: wait(event)
            MasterTime      for `timeout`, without looking at the answer: wait(time)

        MasterEvent does what MasterTimeout does with a slave that answers in time, and MasterTime what it
        does with a slave that is too slow, so they show what the timeout itself costs in each case. A wait
        with a timeout puts an entry in the time queue that is of no use once the answer has arrived.
    */
    template <typename T>
    DSPSIM_MODULE(MasterTimeout)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Output<T> req{"req"};
        dspsim::Input<T> ack{"ack"};

        MasterTimeout(::dspsim::ModuleName, uint64_t gap, uint64_t timeout) : gap_(gap), timeout_(timeout)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                // The count makes every request differ from the one before it.
                req.write(in.read() + ++transfers_);
                if ((co_await wait(ack.change(), timeout_)) == dspsim::WaitResult::Triggered)
                {
                    out.write(ack.read());
                }
                else
                {
                    // No answer in time: pass on the request, marked as failed.
                    out.write(~req.read());
                }
                co_await wait(gap_);
            }
        }

    private:
        uint64_t gap_;
        uint64_t timeout_;
        T transfers_ = 0;
    };

    template <typename T>
    DSPSIM_MODULE(MasterEvent)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Output<T> req{"req"};
        dspsim::Input<T> ack{"ack"};

        MasterEvent(::dspsim::ModuleName, uint64_t gap, uint64_t) : gap_(gap)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                req.write(in.read() + ++transfers_);
                co_await wait(ack.change());
                out.write(ack.read());
                co_await wait(gap_);
            }
        }

    private:
        uint64_t gap_;
        T transfers_ = 0;
    };

    template <typename T>
    DSPSIM_MODULE(MasterTime)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Output<T> req{"req"};
        dspsim::Input<T> ack{"ack"};

        MasterTime(::dspsim::ModuleName, uint64_t gap, uint64_t timeout) : gap_(gap), timeout_(timeout)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                req.write(in.read() + ++transfers_);
                co_await wait(timeout_);
                out.write(~req.read());
                co_await wait(gap_);
            }
        }

    private:
        uint64_t gap_;
        uint64_t timeout_;
        T transfers_ = 0;
    };

    template <typename T>
    DSPSIM_MODULE(Slave)
    {
    public:
        dspsim::Input<T> req{"req"};
        dspsim::Output<T> ack{"ack"};

        Slave(::dspsim::ModuleName, uint64_t latency) : latency_(latency)
        {
            DSPSIM_CORO(run);
        }

        dspsim::Task<> run()
        {
            while (true)
            {
                co_await wait(req.change());
                co_await wait(latency_);
                ack.write(req.read() + 1);
            }
        }

    private:
        uint64_t latency_;
    };

    /// A master of type M connected to its slave.
    template <typename M, typename T>
    DSPSIM_MODULE(BusPair)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::Signal<T> req{"req"};
        dspsim::Signal<T> ack{"ack"};

        M master;
        Slave<T> slave;

        BusPair(::dspsim::ModuleName, uint64_t gap, uint64_t latency, uint64_t timeout)
            : master("master", gap, timeout), slave("slave", latency)
        {
            master.in.bind(in);
            master.out.bind(out);
            master.req.bind(req);
            master.ack.bind(ack);
            slave.req.bind(req);
            slave.ack.bind(ack);
        }
    };

    /*
        A propagation delay: the output follows the input `delay` time units later, the usual way to model
        the delay of a gate or a wire behaviourally. In a chain, a change of the input travels from stage to
        stage, and every stage it reaches is a time step of its own with a single event. A stage does not
        look at its input while it waits for the delay: changes that follow each other faster than that
        become one change of the output.
    */
    /// Method process without static sensitivity, which waits for the input and for the delay in turn.
    template <typename T>
    DSPSIM_MODULE(DelayMethod)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DelayMethod(::dspsim::ModuleName, uint64_t delay) : delay_(delay)
        {
            DSPSIM_METHOD(eval);
        }

        void eval()
        {
            if (changed_)
            {
                // The input has changed: come back when the delay has passed.
                next_trigger(delay_);
            }
            else
            {
                // The delay has passed, or the simulation starts: drive the output and wait for a change.
                out.write(in.read());
                next_trigger(in.change());
            }
            changed_ = !changed_;
        }

    private:
        uint64_t delay_;
        // True if the next evaluation is the one that the change of the input triggers.
        bool changed_ = false;
    };

    /// Coroutine process that waits for the input and for the delay in turn.
    template <typename T>
    DSPSIM_MODULE(DelayCoro)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DelayCoro(::dspsim::ModuleName, uint64_t delay) : delay_(delay)
        {
            DSPSIM_CORO(eval);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                out.write(in.read());
                co_await wait(in.change());
                co_await wait(delay_);
            }
        }

    private:
        uint64_t delay_;
    };

    /*
        Stages M in series, one per element of `times`: stage i is constructed as M(name, times[i], args...),
        so every stage has a time of its own (a period, a delay) and the other arguments in common.

            in -> stage0 -> sig0 -> stage1 -> sig1 -> ... -> out

        The stages and their signals are constructed back to back in one block of memory, like the levels of
        a Chain (see LevelStorage), and the SystemC chain does the same.
    */
    template <typename M, typename T>
    DSPSIM_MODULE(TimedChain)
    {
    public:
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        template <typename... Args>
        TimedChain(::dspsim::ModuleName, const std::vector<uint64_t> &times, const Args &...args)
            : storage_(static_cast<int>(times.size()))
        {
            links_.reserve(times.size());
            for (size_t i = 0; i < times.size(); ++i)
            {
                Link *link = new (storage_.next()) Link(i, times[i], args...);
                if (i == 0)
                {
                    link->stage.in.bind(in);
                }
                else
                {
                    link->stage.in.bind(links_.back()->sig);
                }
                if (i + 1 == times.size())
                {
                    link->stage.out.bind(out);
                }
                else
                {
                    link->stage.out.bind(link->sig);
                }
                links_.push_back(link);
            }
        }
        TimedChain(const TimedChain &) = delete;
        TimedChain &operator=(const TimedChain &) = delete;
        ~TimedChain()
        {
            for (size_t i = links_.size(); i > 0; --i)
            {
                links_[i - 1]->~Link();
            }
        }

    private:
        /// A stage and the signal it drives, which the last stage leaves unused.
        struct Link
        {
            template <typename... Args>
            explicit Link(size_t index, const Args &...args)
                : sig("sig" + std::to_string(index)), stage("stage" + std::to_string(index), args...)
            {
            }

            dspsim::Signal<T> sig;
            M stage;
        };

        LevelStorage<Link> storage_;
        std::vector<Link *> links_;
    };
}
