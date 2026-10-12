/*
    SystemC counterparts of timing.h. A method process is an SC_METHOD and a coroutine process an SC_THREAD.
    Times are given in the time units of the benchmarks, which are 1 ns here.
*/
#pragma once
#include "level_storage.h"

#include <systemc>

#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <vector>

namespace benchmarks
{
    /// A time in time units. The models convert their times once, in the constructor.
    inline sc_core::sc_time timing_units(uint64_t time)
    {
        return sc_core::sc_time(static_cast<double>(time), sc_core::SC_NS);
    }

    /// SC_METHOD without static sensitivity: next_trigger(time) on every evaluation.
    template <typename T>
    SC_MODULE(TimerMethodSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        TimerMethodSC(sc_core::sc_module_name, uint64_t period) : period_(timing_units(period))
        {
            SC_METHOD(eval);
        }

        void eval()
        {
            out.write(in.read() + ++count_);
            next_trigger(period_);
        }

    private:
        sc_core::sc_time period_;
        T count_ = 0;
    };

    /// SC_THREAD that waits for the time.
    template <typename T>
    SC_MODULE(TimerThreadSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        TimerThreadSC(sc_core::sc_module_name, uint64_t period) : period_(timing_units(period))
        {
            SC_THREAD(eval);
        }

        void eval()
        {
            while (true)
            {
                out.write(in.read() + ++count_);
                wait(period_);
            }
        }

    private:
        sc_core::sc_time period_;
        T count_ = 0;
    };

    /// A clock domain with an SC_METHOD.
    template <typename T>
    SC_MODULE(DomainMethodSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_clock clk;

        DomainMethodSC(sc_core::sc_module_name, uint64_t period) : clk("clk", timing_units(period))
        {
            SC_METHOD(eval);
            sensitive << clk.posedge_event();
            dont_initialize();
        }

        void eval()
        {
            out.write(in.read() + ++count_);
        }

    private:
        T count_ = 0;
    };

    /// A clock domain with an SC_THREAD.
    template <typename T>
    SC_MODULE(DomainThreadSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_clock clk;

        DomainThreadSC(sc_core::sc_module_name, uint64_t period) : clk("clk", timing_units(period))
        {
            SC_THREAD(eval);
            sensitive << clk.posedge_event();
        }

        void eval()
        {
            while (true)
            {
                wait();
                out.write(in.read() + ++count_);
            }
        }

    private:
        T count_ = 0;
    };

    /// Bus master that waits for the answer with a timeout: wait(timeout, event).
    template <typename T>
    SC_MODULE(MasterTimeoutSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_out<T> req{"req"};
        sc_core::sc_in<T> ack{"ack"};

        MasterTimeoutSC(sc_core::sc_module_name, uint64_t gap, uint64_t timeout)
            : gap_(timing_units(gap)), timeout_(timing_units(timeout))
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                req.write(in.read() + ++transfers_);
                wait(timeout_, ack.value_changed_event());
                if (!timed_out())
                {
                    out.write(ack.read());
                }
                else
                {
                    out.write(~req.read());
                }
                wait(gap_);
            }
        }

    private:
        sc_core::sc_time gap_;
        sc_core::sc_time timeout_;
        T transfers_ = 0;
    };

    /// Bus master that waits for the answer without a timeout: wait(event).
    template <typename T>
    SC_MODULE(MasterEventSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_out<T> req{"req"};
        sc_core::sc_in<T> ack{"ack"};

        MasterEventSC(sc_core::sc_module_name, uint64_t gap, uint64_t) : gap_(timing_units(gap))
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                req.write(in.read() + ++transfers_);
                wait(ack.value_changed_event());
                out.write(ack.read());
                wait(gap_);
            }
        }

    private:
        sc_core::sc_time gap_;
        T transfers_ = 0;
    };

    /// Bus master that waits for the timeout without looking at the answer: wait(time).
    template <typename T>
    SC_MODULE(MasterTimeSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_out<T> req{"req"};
        sc_core::sc_in<T> ack{"ack"};

        MasterTimeSC(sc_core::sc_module_name, uint64_t gap, uint64_t timeout)
            : gap_(timing_units(gap)), timeout_(timing_units(timeout))
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                req.write(in.read() + ++transfers_);
                wait(timeout_);
                out.write(~req.read());
                wait(gap_);
            }
        }

    private:
        sc_core::sc_time gap_;
        sc_core::sc_time timeout_;
        T transfers_ = 0;
    };

    template <typename T>
    SC_MODULE(SlaveSC)
    {
        sc_core::sc_in<T> req{"req"};
        sc_core::sc_out<T> ack{"ack"};

        SlaveSC(sc_core::sc_module_name, uint64_t latency) : latency_(timing_units(latency))
        {
            SC_THREAD(run);
        }

        void run()
        {
            while (true)
            {
                wait(req.value_changed_event());
                wait(latency_);
                ack.write(req.read() + 1);
            }
        }

    private:
        sc_core::sc_time latency_;
    };

    /// A master of type M connected to its slave.
    template <typename M, typename T>
    SC_MODULE(BusPairSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_signal<T> req{"req"};
        sc_core::sc_signal<T> ack{"ack"};

        M master;
        SlaveSC<T> slave;

        BusPairSC(sc_core::sc_module_name, uint64_t gap, uint64_t latency, uint64_t timeout)
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

    /// Transport delay as an SC_METHOD without static sensitivity.
    template <typename T>
    SC_MODULE(DelayMethodSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        DelayMethodSC(sc_core::sc_module_name, uint64_t delay) : delay_(timing_units(delay))
        {
            SC_METHOD(eval);
        }

        void eval()
        {
            if (changed_)
            {
                next_trigger(delay_);
            }
            else
            {
                out.write(in.read());
                next_trigger(in.value_changed_event());
            }
            changed_ = !changed_;
        }

    private:
        sc_core::sc_time delay_;
        bool changed_ = false;
    };

    /// Transport delay as an SC_THREAD.
    template <typename T>
    SC_MODULE(DelayThreadSC)
    {
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        DelayThreadSC(sc_core::sc_module_name, uint64_t delay) : delay_(timing_units(delay))
        {
            SC_THREAD(eval);
        }

        void eval()
        {
            while (true)
            {
                out.write(in.read());
                wait(in.value_changed_event());
                wait(delay_);
            }
        }

    private:
        sc_core::sc_time delay_;
    };

    /// Stages M in series, one per element of `times`. See TimedChain in timing.h.
    template <typename M, typename T>
    class TimedChainSC : public sc_core::sc_module
    {
    public:
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        template <typename... Args>
        TimedChainSC(sc_core::sc_module_name name, const std::vector<uint64_t> &times, const Args &...args)
            : sc_core::sc_module(name), storage_(static_cast<int>(times.size()))
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
        ~TimedChainSC() override
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
                : sig(("sig" + std::to_string(index)).c_str()),
                  stage(("stage" + std::to_string(index)).c_str(), args...)
            {
            }

            sc_core::sc_signal<T> sig;
            M stage;
        };

        LevelStorage<Link> storage_;
        std::vector<Link *> links_;
    };
}
