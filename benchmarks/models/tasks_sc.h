/*
    SystemC counterparts of tasks.h. A coroutine process is an SC_THREAD, and a task is an ordinary member
    function that calls wait(): it runs on the stack of the thread that calls it.
*/
#pragma once
#include <systemc>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    /// The transaction is written in the thread.
    template <typename T>
    SC_MODULE(WorkerInlineSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(WorkerInlineSC)
        {
            SC_THREAD(body);
            sensitive << clk.pos();
        }

        void body()
        {
            while (true)
            {
                wait();
                const T x = in.read();
                wait();
                out.write(x + 1);
            }
        }
    };

    /// The transaction is one function that the thread calls.
    template <typename T>
    SC_MODULE(WorkerTaskSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(WorkerTaskSC)
        {
            SC_THREAD(body);
            sensitive << clk.pos();
        }

        T transfer()
        {
            wait();
            const T x = in.read();
            wait();
            return x + 1;
        }

        void body()
        {
            while (true)
            {
                out.write(transfer());
            }
        }
    };

    /// The transaction is made of functions nested three deep, and only the innermost one waits.
    template <typename T>
    SC_MODULE(WorkerNestedSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(WorkerNestedSC)
        {
            SC_THREAD(body);
            sensitive << clk.pos();
        }

        void tick()
        {
            wait();
        }

        T sample()
        {
            tick();
            return in.read();
        }

        T transfer()
        {
            const T x = sample();
            tick();
            return x + 1;
        }

        void body()
        {
            while (true)
            {
                out.write(transfer());
            }
        }
    };

    /// The slave of the bus: a register file, as a clocked SC_METHOD. See BusSlave in tasks.h.
    template <typename T>
    SC_MODULE(BusSlaveSC)
    {
        /// Bits 3:0 of an address select the register, bits 5:4 are the number of wait states.
        static constexpr uint32_t address_mask = 63;

        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<bool> req{"req"};
        sc_core::sc_in<bool> we{"we"};
        sc_core::sc_in<uint32_t> addr{"addr"};
        sc_core::sc_in<T> wdata{"wdata"};
        sc_core::sc_out<bool> ack{"ack"};
        sc_core::sc_out<T> rdata{"rdata"};

        SC_CTOR(BusSlaveSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
        }

        void eval()
        {
            if (ack.read())
            {
                // The master sees the acknowledge on this edge. A request that is still there is the next access.
                ack.write(false);
            }
            else if (req.read())
            {
                const uint32_t address = addr.read();
                if (!busy_)
                {
                    busy_ = true;
                    wait_states_ = (address >> 4) & 3;
                }
                if (wait_states_ > 0)
                {
                    --wait_states_;
                }
                else
                {
                    T &reg = regs_[address & 15];
                    if (we.read())
                    {
                        reg = wdata.read();
                    }
                    else
                    {
                        rdata.write(reg);
                    }
                    ack.write(true);
                    busy_ = false;
                }
            }
        }

    private:
        std::array<T, 16> regs_{};
        bool busy_ = false;
        uint32_t wait_states_ = 0;
    };

    /// The master of the bus: it has no process of its own. Its functions run in the thread that calls
    /// them, so they wait for the clock with dynamic sensitivity.
    template <typename T>
    SC_MODULE(BusMasterSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_out<bool> req{"req"};
        sc_core::sc_out<bool> we{"we"};
        sc_core::sc_out<uint32_t> addr{"addr"};
        sc_core::sc_out<T> wdata{"wdata"};
        sc_core::sc_in<bool> ack{"ack"};
        sc_core::sc_in<T> rdata{"rdata"};

        SC_CTOR(BusMasterSC) {}

        /// Write a register. Returns on the clock edge at which the slave acknowledges.
        void write(uint32_t address, T data)
        {
            addr.write(address);
            wdata.write(data);
            we.write(true);
            req.write(true);
            do
            {
                wait(clk.posedge_event());
            } while (!ack.read());
            req.write(false);
        }

        /// Read a register.
        T read(uint32_t address)
        {
            addr.write(address);
            we.write(false);
            req.write(true);
            do
            {
                wait(clk.posedge_event());
            } while (!ack.read());
            req.write(false);
            return rdata.read();
        }
    };

    /*
        A master and a slave, connected.

        An sc_signal accepts writes from one process only, unless it is declared otherwise. The signals of
        the master are written by the thread that calls its functions, so a testbench that calls them from
        more than one thread, one after the other, makes them SC_MANY_WRITERS signals. Those are updated
        after every write, changed or not, to forget who wrote. dspsim does not check who writes a signal.
    */
    template <typename T, sc_core::sc_writer_policy POLICY = sc_core::SC_ONE_WRITER>
    SC_MODULE(BusSC)
    {
        sc_core::sc_in<bool> clk{"clk"};

        sc_core::sc_signal<bool, POLICY> req{"req"};
        sc_core::sc_signal<bool, POLICY> we{"we"};
        sc_core::sc_signal<uint32_t, POLICY> addr{"addr"};
        sc_core::sc_signal<T, POLICY> wdata{"wdata"};
        sc_core::sc_signal<bool> ack{"ack"};
        sc_core::sc_signal<T> rdata{"rdata"};

        BusMasterSC<T> master{"master"};
        BusSlaveSC<T> slave{"slave"};

        SC_CTOR(BusSC)
        {
            master.clk(clk);
            master.req(req);
            master.we(we);
            master.addr(addr);
            master.wdata(wdata);
            master.ack(ack);
            master.rdata(rdata);

            slave.clk(clk);
            slave.req(req);
            slave.we(we);
            slave.addr(addr);
            slave.wdata(wdata);
            slave.ack(ack);
            slave.rdata(rdata);
        }
    };

    /// The accesses are the functions of the master, called by an SC_THREAD.
    template <typename T>
    SC_MODULE(BusTaskSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        BusSC<T> bus{"bus"};

        SC_CTOR(BusTaskSC)
        {
            bus.clk(clk);
            SC_THREAD(body);
        }

        void body()
        {
            T sum = 0;
            while (true)
            {
                const T x = in.read();
                bus.master.write(x & BusSlaveSC<T>::address_mask, x);
                sum += bus.master.read((x >> 6) & BusSlaveSC<T>::address_mask);
                out.write(sum);
            }
        }
    };

    /// The same master as a state machine in an SC_METHOD, which drives the bus signals itself.
    template <typename T>
    SC_MODULE(BusMethodSC)
    {
    private:
        enum class State
        {
            Start,
            Write,
            Read
        };

    public:
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        sc_core::sc_signal<bool> req{"req"};
        sc_core::sc_signal<bool> we{"we"};
        sc_core::sc_signal<uint32_t> addr{"addr"};
        sc_core::sc_signal<T> wdata{"wdata"};
        sc_core::sc_signal<bool> ack{"ack"};
        sc_core::sc_signal<T> rdata{"rdata"};

        BusSlaveSC<T> slave{"slave"};

        SC_CTOR(BusMethodSC)
        {
            slave.clk(clk);
            slave.req(req);
            slave.we(we);
            slave.addr(addr);
            slave.wdata(wdata);
            slave.ack(ack);
            slave.rdata(rdata);

            // The evaluation at the start of the simulation requests the first access, like the body of BusTaskSC.
            SC_METHOD(eval);
            sensitive << clk.pos();
        }

        void eval()
        {
            switch (state_)
            {
            case State::Start:
                start_write();
                break;
            case State::Write:
                if (ack.read())
                {
                    // The request stays up: the read follows the write without a gap.
                    addr.write((x_ >> 6) & BusSlaveSC<T>::address_mask);
                    we.write(false);
                    state_ = State::Read;
                }
                break;
            case State::Read:
                if (ack.read())
                {
                    sum_ += rdata.read();
                    out.write(sum_);
                    start_write();
                }
                break;
            }
        }

    private:
        void start_write()
        {
            x_ = in.read();
            addr.write(x_ & BusSlaveSC<T>::address_mask);
            wdata.write(x_);
            we.write(true);
            req.write(true);
            state_ = State::Write;
        }

    private:
        State state_ = State::Start;
        T x_ = 0;
        T sum_ = 0;
    };

    /// A job is a thread process: sc_spawn(). See SpawnCoro in tasks.h.
    template <typename T>
    SC_MODULE(SpawnThreadSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(SpawnThreadSC)
        {
            SC_THREAD(dispatch);
            sensitive << clk.pos();
            SC_METHOD(collect);
            sensitive << done_;
            dont_initialize();
        }

        void dispatch()
        {
            while (true)
            {
                wait();
                // SystemC wants a name of its own for every process.
                const T x = in.read();
                sc_core::sc_spawn([this, x]
                                  { job(x); }, sc_core::sc_gen_unique_name("job"));
            }
        }

        void job(T x)
        {
            for (uint32_t cycles = 1 + (x & 7); cycles > 0; --cycles)
            {
                wait(clk.posedge_event());
            }
            sum_ += x;
            done_.notify(sc_core::SC_ZERO_TIME);
        }

        void collect()
        {
            out.write(sum_);
        }

    private:
        sc_core::sc_event done_;
        T sum_ = 0;
    };

    /// A job is a method process without static sensitivity: sc_spawn() with spawn_method(). See SpawnMethod
    /// in tasks.h.
    template <typename T>
    SC_MODULE(SpawnMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(SpawnMethodSC)
        {
            SC_METHOD(dispatch);
            sensitive << clk.pos();
            dont_initialize();
            SC_METHOD(collect);
            sensitive << done_;
            dont_initialize();
        }

        void dispatch()
        {
            const T x = in.read();
            sc_core::sc_spawn_options options;
            options.spawn_method();
            sc_core::sc_spawn([this, x, cycles = static_cast<uint32_t>(1 + (x & 7))]() mutable
                              { job(x, cycles); }, sc_core::sc_gen_unique_name("job"), &options);
        }

        /// One evaluation of a job. `cycles` is the job's state: the clock cycles it still has to wait.
        void job(T x, uint32_t &cycles)
        {
            if (cycles > 0)
            {
                --cycles;
                next_trigger(clk.posedge_event());
            }
            else
            {
                sum_ += x;
                done_.notify(sc_core::SC_ZERO_TIME);
            }
        }

        void collect()
        {
            out.write(sum_);
        }

    private:
        sc_core::sc_event done_;
        T sum_ = 0;
    };

    /// A timer: an SC_METHOD without static sensitivity that waits with next_trigger(time). See Timer in tasks.h.
    SC_MODULE(TimerSC)
    {
        TimerSC(sc_core::sc_module_name name, const sc_core::sc_time &first, const sc_core::sc_time &period,
                uint64_t &expirations)
            : sc_core::sc_module(name), next_(first), period_(period), expirations_(expirations)
        {
            SC_METHOD(eval);
        }

        void eval()
        {
            // The evaluation at the start of the simulation counts as well.
            ++expirations_;
            next_trigger(next_);
            next_ = period_;
        }

    private:
        sc_core::sc_time next_;
        sc_core::sc_time period_;
        uint64_t &expirations_;
    };

    /// `count` timers that expire one after the other, one every `interval` time units (nanoseconds).
    SC_MODULE(TimersSC)
    {
        /// The number of times a timer has expired.
        uint64_t expirations = 0;

        TimersSC(sc_core::sc_module_name name, int count, uint64_t interval)
            : sc_core::sc_module(name)
        {
            for (int i = 0; i < count; ++i)
            {
                const std::string timer_name = "timer" + std::to_string(i);
                const double first = static_cast<double>(interval * static_cast<uint64_t>(i + 1));
                const double period = static_cast<double>(interval * static_cast<uint64_t>(count));
                timers_.push_back(std::make_unique<TimerSC>(timer_name.c_str(), sc_core::sc_time(first, sc_core::SC_NS),
                                                            sc_core::sc_time(period, sc_core::SC_NS), expirations));
            }
        }

    private:
        std::vector<std::unique_ptr<TimerSC>> timers_;
    };
}
