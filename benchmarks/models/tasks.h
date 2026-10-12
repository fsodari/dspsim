/*
    dspsim models for the tasks benchmarks: what coroutine tasks are used for. tasks_sc.h has the SystemC
    counterparts.

        Worker*   a process that repeats a transaction, written inline, as one task and as nested tasks
        Bus*      a bus master with read() and write() tasks and a register file as its slave
        Spawn*    a dispatcher that creates a process for every job
        Timers    processes with a pending time event, as the background of the run_until benchmarks

    A dspsim coroutine is stackless: a function that waits is a coroutine itself, a Task<T> with a frame of
    its own that is allocated when it is called, and the process resumes in the innermost one. A SystemC
    thread has a stack, so there the same functions are ordinary functions.
*/
#pragma once
#include <dspsim/dspsim.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    /*
        A worker repeats one transaction: sample the input on a clock edge and drive the result on the next
        one. It stands for the bus functional models and protocol layers of a testbench, where a transaction
        is a function that the process calls, and that calls further functions.

        The three workers produce the same output. They differ in the number of tasks a transaction is made
        of, which is the number of coroutine frames the engine allocates, enters and frees for it.
    */
    /// The transaction is written in the process body: no task.
    template <typename T>
    DSPSIM_MODULE(WorkerInline)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(WorkerInline)
        {
            DSPSIM_CORO(body)
                ->always(clk.pos());
        }

        dspsim::Task<> body()
        {
            while (true)
            {
                co_await wait();
                const T x = in.read();
                co_await wait();
                out.write(x + 1);
            }
        }
    };

    /// The transaction is one task that the process body awaits.
    template <typename T>
    DSPSIM_MODULE(WorkerTask)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(WorkerTask)
        {
            DSPSIM_CORO(body)
                ->always(clk.pos());
        }

        dspsim::Task<T> transfer()
        {
            co_await wait();
            const T x = in.read();
            co_await wait();
            co_return x + 1;
        }

        dspsim::Task<> body()
        {
            while (true)
            {
                out.write(co_await transfer());
            }
        }
    };

    /// The transaction is made of tasks nested three deep (transfer, sample, tick), and only the innermost
    /// one waits: four tasks per transaction.
    template <typename T>
    DSPSIM_MODULE(WorkerNested)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(WorkerNested)
        {
            DSPSIM_CORO(body)
                ->always(clk.pos());
        }

        dspsim::Task<> tick()
        {
            co_await wait();
        }

        dspsim::Task<T> sample()
        {
            co_await tick();
            co_return in.read();
        }

        dspsim::Task<T> transfer()
        {
            const T x = co_await sample();
            co_await tick();
            co_return x + 1;
        }

        dspsim::Task<> body()
        {
            while (true)
            {
                out.write(co_await transfer());
            }
        }
    };

    /*
        A bus with a request/acknowledge handshake, the main use of tasks: the master offers read() and
        write() as tasks, and whoever awaits them, a process of the design or the testbench through
        Context::run_until(), sees a function call that takes simulated time.

            master: req, we, addr, wdata  ->  slave
            slave:  ack, rdata            ->  master

        The master holds req until it sees ack on a clock edge. The slave raises ack for one clock cycle,
        after as many wait states as the address asks for, so an access takes 2 to 5 clock cycles.

        For the engine, an access is one task, and every clock cycle of it is a wait on the clock event with
        dynamic sensitivity: a task cannot rely on the static sensitivity of the process that awaits it.
    */
    /// The slave: a register file, as a clocked method process.
    template <typename T>
    DSPSIM_MODULE(BusSlave)
    {
    public:
        /// Bits 3:0 of an address select the register, bits 5:4 are the number of wait states.
        static constexpr uint32_t address_mask = 63;

        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<uint8_t> req{"req"};
        dspsim::Input<uint8_t> we{"we"};
        dspsim::Input<uint32_t> addr{"addr"};
        dspsim::Input<T> wdata{"wdata"};
        dspsim::Output<uint8_t> ack{"ack"};
        dspsim::Output<T> rdata{"rdata"};

        DSPSIM_CTOR(BusSlave)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            if (ack.read())
            {
                // The master sees the acknowledge on this edge. A request that is still there is the next access.
                ack.write(0);
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
                    ack.write(1);
                    busy_ = false;
                }
            }
        }

    private:
        std::array<T, 16> regs_{};
        bool busy_ = false;
        uint32_t wait_states_ = 0;
    };

    /// The master: it has no process of its own. Its tasks run in the process that awaits them, so they
    /// wait for the clock with dynamic sensitivity.
    template <typename T>
    DSPSIM_MODULE(BusMaster)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Output<uint8_t> req{"req"};
        dspsim::Output<uint8_t> we{"we"};
        dspsim::Output<uint32_t> addr{"addr"};
        dspsim::Output<T> wdata{"wdata"};
        dspsim::Input<uint8_t> ack{"ack"};
        dspsim::Input<T> rdata{"rdata"};

        DSPSIM_CTOR(BusMaster) {}

        /// Write a register. Completes on the clock edge at which the slave acknowledges.
        dspsim::Task<> write(uint32_t address, T data)
        {
            addr.write(address);
            wdata.write(data);
            we.write(1);
            req.write(1);
            do
            {
                co_await wait(clk.pos());
            } while (!ack.read());
            req.write(0);
        }

        /// Read a register.
        dspsim::Task<T> read(uint32_t address)
        {
            addr.write(address);
            we.write(0);
            req.write(1);
            do
            {
                co_await wait(clk.pos());
            } while (!ack.read());
            req.write(0);
            co_return rdata.read();
        }
    };

    /// A master and a slave, connected. The user of the bus calls the tasks of `master`.
    template <typename T>
    DSPSIM_MODULE(Bus)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};

        dspsim::Signal<uint8_t> req{"req"};
        dspsim::Signal<uint8_t> we{"we"};
        dspsim::Signal<uint32_t> addr{"addr"};
        dspsim::Signal<T> wdata{"wdata"};
        dspsim::Signal<uint8_t> ack{"ack"};
        dspsim::Signal<T> rdata{"rdata"};

        BusMaster<T> master{"master"};
        BusSlave<T> slave{"slave"};

        DSPSIM_CTOR(Bus)
        {
            master.clk.bind(clk);
            master.req.bind(req);
            master.we.bind(we);
            master.addr.bind(addr);
            master.wdata.bind(wdata);
            master.ack.bind(ack);
            master.rdata.bind(rdata);

            slave.clk.bind(clk);
            slave.req.bind(req);
            slave.we.bind(we);
            slave.addr.bind(addr);
            slave.wdata.bind(wdata);
            slave.ack.bind(ack);
            slave.rdata.bind(rdata);
        }
    };

    /*
        A bus and the process that uses it: for every input value, write it to one register and read
        another one back, which holds an earlier input. The output is the sum of the values read, so it
        changes with every pair of accesses and depends on what the registers held.
    */
    /// The accesses are the tasks of the master, awaited by a coroutine process.
    template <typename T>
    DSPSIM_MODULE(BusTask)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        Bus<T> bus{"bus"};

        DSPSIM_CTOR(BusTask)
        {
            bus.clk.bind(clk);
            DSPSIM_CORO(body);
        }

        dspsim::Task<> body()
        {
            T sum = 0;
            while (true)
            {
                const T x = in.read();
                co_await bus.master.write(x & BusSlave<T>::address_mask, x);
                sum += co_await bus.master.read((x >> 6) & BusSlave<T>::address_mask);
                out.write(sum);
            }
        }
    };

    /// The same master as a state machine in a method process, which drives the bus signals itself: what
    /// the tasks replace. It changes the signals on the same clock edges as BusTask.
    template <typename T>
    DSPSIM_MODULE(BusMethod)
    {
    private:
        enum class State
        {
            Start,
            Write,
            Read
        };

    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        dspsim::Signal<uint8_t> req{"req"};
        dspsim::Signal<uint8_t> we{"we"};
        dspsim::Signal<uint32_t> addr{"addr"};
        dspsim::Signal<T> wdata{"wdata"};
        dspsim::Signal<uint8_t> ack{"ack"};
        dspsim::Signal<T> rdata{"rdata"};

        BusSlave<T> slave{"slave"};

        DSPSIM_CTOR(BusMethod)
        {
            slave.clk.bind(clk);
            slave.req.bind(req);
            slave.we.bind(we);
            slave.addr.bind(addr);
            slave.wdata.bind(wdata);
            slave.ack.bind(ack);
            slave.rdata.bind(rdata);

            // The evaluation at the start of the simulation requests the first access, like the body of BusTask.
            DSPSIM_METHOD(eval)
                ->always(clk.pos());
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
                    addr.write((x_ >> 6) & BusSlave<T>::address_mask);
                    we.write(0);
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
            addr.write(x_ & BusSlave<T>::address_mask);
            wdata.write(x_);
            we.write(1);
            req.write(1);
            state_ = State::Write;
        }

    private:
        State state_ = State::Start;
        T x_ = 0;
        T sum_ = 0;
    };

    /*
        A dispatcher that starts a job for every input value, one per clock cycle. A job lasts 1 to 8 clock
        cycles, depending on its value, adds the value to a sum and ends: several jobs are alive at a time.
        This is how a testbench models concurrent activity whose amount is not known in advance, such as one
        process per packet or per request in flight. It stresses the creation of processes while the
        simulation runs and what the engine does with a process that has ended.

        The jobs that end on a clock edge add to the sum in the same delta cycle, in any order. They notify an
        event, and the process that puts the sum on the output runs in the delta cycle after it.
    */
    /// A job is a coroutine process that the context destroys when it ends: Context::spawn().
    template <typename T>
    DSPSIM_MODULE(SpawnCoro)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(SpawnCoro)
        {
            DSPSIM_CORO(dispatch)
                ->always(clk.pos());
            DSPSIM_METHOD(collect)
                ->always(done_)
                ->initialize(false);
        }

        dspsim::Task<> dispatch()
        {
            while (true)
            {
                co_await wait();
                context()->spawn(job(in.read()), "job");
            }
        }

        dspsim::Task<> job(T x)
        {
            for (uint32_t cycles = 1 + (x & 7); cycles > 0; --cycles)
            {
                co_await wait(clk.pos());
            }
            sum_ += x;
            done_.notify();
        }

        void collect()
        {
            out.write(sum_);
        }

    private:
        dspsim::SensitivityEvent done_{context()};
        T sum_ = 0;
    };

    /// A job is a function process without static sensitivity: Context::register_process_func(). It is
    /// evaluated when it is created and then on every clock edge it asks for with next_trigger(), so it ends
    /// by not asking again.
    template <typename T>
    DSPSIM_MODULE(SpawnMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(SpawnMethod)
        {
            DSPSIM_METHOD(dispatch)
                ->always(clk.pos())
                ->initialize(false);
            DSPSIM_METHOD(collect)
                ->always(done_)
                ->initialize(false);
        }

        void dispatch()
        {
            const T x = in.read();
            context()->register_process_func([this, x, cycles = static_cast<uint32_t>(1 + (x & 7))]() mutable
                                             { job(x, cycles); }, "job");
        }

        /// One evaluation of a job. `cycles` is the job's state: the clock cycles it still has to wait.
        void job(T x, uint32_t &cycles)
        {
            if (cycles > 0)
            {
                --cycles;
                next_trigger(clk.pos());
            }
            else
            {
                sum_ += x;
                done_.notify();
            }
        }

        void collect()
        {
            out.write(sum_);
        }

    private:
        dspsim::SensitivityEvent done_{context()};
        T sum_ = 0;
    };

    /*
        Timers that run in the background of a design: timeouts, watchdogs, periodic housekeeping. A timer is
        a method process without static sensitivity that waits with next_trigger(time), so each of them
        always has one time event pending.
    */
    DSPSIM_MODULE(Timer)
    {
    public:
        /// The timer expires after `first` time units and then every `period`. It counts in `expirations`.
        Timer(::dspsim::ModuleName, uint64_t first, uint64_t period, uint64_t &expirations)
            : next_(first), period_(period), expirations_(expirations)
        {
            DSPSIM_METHOD(eval);
        }

        void eval()
        {
            // The evaluation at the start of the simulation counts as well.
            ++expirations_;
            next_trigger(next_);
            next_ = period_;
        }

    private:
        uint64_t next_;
        uint64_t period_;
        uint64_t &expirations_;
    };

    /// `count` timers with the same period that expire one after the other, one every `interval` time units.
    DSPSIM_MODULE(Timers)
    {
    public:
        /// The number of times a timer has expired.
        uint64_t expirations = 0;

        Timers(::dspsim::ModuleName, int count, uint64_t interval)
        {
            for (int i = 0; i < count; ++i)
            {
                const uint64_t first = interval * static_cast<uint64_t>(i + 1);
                const uint64_t period = interval * static_cast<uint64_t>(count);
                timers_.push_back(std::make_unique<Timer>("timer" + std::to_string(i), first, period, expirations));
            }
        }

    private:
        std::vector<std::unique_ptr<Timer>> timers_;
    };
}
