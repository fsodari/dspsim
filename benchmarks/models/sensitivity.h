/*
    dspsim models for the sensitivity benchmarks: blocks with one process each, in each process kind, and the
    designs that connect them. sensitivity_sc.h has the SystemC counterparts.

        Regs    a bank of registers loaded from one input: what changes the signals the others watch
        Sum     the sum of N inputs: one process that is sensitive to N signals
        Add     a two-input adder: the node of a tree
        Tap     one of the N blocks that watch the same signal
*/
#pragma once
#include <dspsim/dspsim.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    /*
        A bank of N registers behind one input, like the registers of a bus slave. On a clock edge, `changes`
        consecutive registers get a new value, starting at the register the input selects. One evaluation
        makes that many signals change, and their events occur, in the same delta cycle: it sets how many of
        the signals that another process is sensitive to change at once.
    */
    /// Method process.
    template <typename T>
    DSPSIM_MODULE(RegsMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::OutputArray<T> q;

        RegsMethod(::dspsim::ModuleName, size_t n, size_t changes) : q{"q", {n}}, changes_(changes)
        {
            // The coroutine kind only writes after a clock edge, so skip the initial evaluation.
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            const T value = in.read();
            size_t index = value % q.size();
            for (size_t i = 0; i < changes_; ++i)
            {
                q.flat(index).write(value + static_cast<T>(i));
                index = index + 1 == q.size() ? 0 : index + 1;
            }
        }

    private:
        size_t changes_;
    };

    /// Coroutine process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(RegsCoro)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::OutputArray<T> q;

        RegsCoro(::dspsim::ModuleName, size_t n, size_t changes) : q{"q", {n}}, changes_(changes)
        {
            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait();
                const T value = in.read();
                size_t index = value % q.size();
                for (size_t i = 0; i < changes_; ++i)
                {
                    q.flat(index).write(value + static_cast<T>(i));
                    index = index + 1 == q.size() ? 0 : index + 1;
                }
            }
        }

    private:
        size_t changes_;
    };

    /*
        The sum of N inputs: combinational logic with many inputs, like the read multiplexer of a bus or an
        interrupt controller. One process is sensitive to N signals. However many of them change in a delta
        cycle, it has to run once: the output includes the number of evaluations, so an engine that evaluates
        it once per event computes another checksum.

        The dynamic kinds wait for a change of any input instead, the way a monitor or an arbiter waits for
        activity on any of its request lines. That subscribes the process to N events on every evaluation,
        and the first event to occur has to take it off the other N - 1.
    */
    /// Method process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(SumMethod)
    {
    public:
        dspsim::InputArray<T> d;
        dspsim::Output<T> out{"out"};

        SumMethod(::dspsim::ModuleName, size_t n) : d{"d", {n}}
        {
            DSPSIM_METHOD(eval)
                ->always(d);
        }

        void eval()
        {
            T sum = ++evals_;
            for (auto &input : d)
            {
                sum += input.read();
            }
            out.write(sum);
        }

    private:
        T evals_ = 0;
    };

    /// Method process with dynamic sensitivity: next_trigger() for the list of inputs, on every evaluation.
    template <typename T>
    DSPSIM_MODULE(SumMethodDynamic)
    {
    public:
        dspsim::InputArray<T> d;
        dspsim::Output<T> out{"out"};

        SumMethodDynamic(::dspsim::ModuleName, size_t n) : d{"d", {n}}
        {
            DSPSIM_METHOD(eval);
        }

        void eval()
        {
            // The list of events is made in the first evaluation. The ports are bound by then, so their
            // events are the events of the signals: before elaboration, a port has events of its own.
            if (any_input_.empty())
            {
                for (auto &input : d)
                {
                    any_input_.push_back(input.change());
                }
            }
            T sum = ++evals_;
            for (auto &input : d)
            {
                sum += input.read();
            }
            out.write(sum);
            next_trigger(any_input_);
        }

    private:
        T evals_ = 0;
        std::vector<std::reference_wrapper<dspsim::SensitivityEvent>> any_input_;
    };

    /// Coroutine process with static sensitivity.
    template <typename T>
    DSPSIM_MODULE(SumCoro)
    {
    public:
        dspsim::InputArray<T> d;
        dspsim::Output<T> out{"out"};

        SumCoro(::dspsim::ModuleName, size_t n) : d{"d", {n}}
        {
            DSPSIM_CORO(eval)
                ->always(d);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                T sum = ++evals_;
                for (auto &input : d)
                {
                    sum += input.read();
                }
                out.write(sum);
                co_await wait();
            }
        }

    private:
        T evals_ = 0;
    };

    /// Coroutine process with dynamic sensitivity: it waits for any of the N events, on every evaluation.
    template <typename T>
    DSPSIM_MODULE(SumCoroDynamic)
    {
    public:
        dspsim::InputArray<T> d;
        dspsim::Output<T> out{"out"};

        SumCoroDynamic(::dspsim::ModuleName, size_t n) : d{"d", {n}}
        {
            DSPSIM_CORO(eval);
        }

        dspsim::Task<> eval()
        {
            // The list of events is made once, when the process starts. The ports are bound by then, so
            // their events are the events of the signals: before elaboration, a port has events of its own.
            std::vector<std::reference_wrapper<dspsim::SensitivityEvent>> any_input;
            for (auto &input : d)
            {
                any_input.push_back(input.change());
            }
            while (true)
            {
                T sum = ++evals_;
                for (auto &input : d)
                {
                    sum += input.read();
                }
                out.write(sum);
                co_await wait(any_input);
            }
        }

    private:
        T evals_ = 0;
    };

    /*
        A two-input adder, the node of an adder tree: a process that is sensitive to both of its inputs. In a
        balanced tree they change in the same delta cycle, one level after the other.
    */
    template <typename T>
    DSPSIM_MODULE(AddMethod)
    {
    public:
        dspsim::Input<T> a{"a"};
        dspsim::Input<T> b{"b"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(AddMethod)
        {
            DSPSIM_METHOD(eval)
                ->always(a, b);
        }

        void eval()
        {
            out.write(a.read() + b.read());
        }
    };

    template <typename T>
    DSPSIM_MODULE(AddCoro)
    {
    public:
        dspsim::Input<T> a{"a"};
        dspsim::Input<T> b{"b"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(AddCoro)
        {
            DSPSIM_CORO(eval)
                ->always(a, b);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                out.write(a.read() + b.read());
                co_await wait();
            }
        }
    };

    /*
        One of the blocks that watch a shared signal, like a peripheral on an address bus: its output follows
        the input, with an offset of its own. The change event of the shared signal has as many subscribers
        as there are taps.
    */
    template <typename T>
    DSPSIM_MODULE(TapMethod)
    {
    public:
        dspsim::Input<T> d{"d"};
        dspsim::Output<T> q{"q"};

        TapMethod(::dspsim::ModuleName, T offset) : offset_(offset)
        {
            DSPSIM_METHOD(eval)
                ->always(d);
        }

        void eval()
        {
            q.write(d.read() + offset_);
        }

    private:
        T offset_;
    };

    template <typename T>
    DSPSIM_MODULE(TapCoro)
    {
    public:
        dspsim::Input<T> d{"d"};
        dspsim::Output<T> q{"q"};

        TapCoro(::dspsim::ModuleName, T offset) : offset_(offset)
        {
            DSPSIM_CORO(eval)
                ->always(d);
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                q.write(d.read() + offset_);
                co_await wait();
            }
        }

    private:
        T offset_;
    };

    /*
        Fan-in: the sum of n registers, of which `changes` change on every clock edge.

            in -> regs -> sigs[n] -> sum -> out

        With changes = n, the n events that the sum is sensitive to all occur in the same delta cycle. With
        changes = 1 a single one occurs, which is the case that waiting for any of n events is meant for.
    */
    template <typename Regs, typename Sum, typename T>
    DSPSIM_MODULE(Fanin)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::SignalArray<T> sigs;

        Regs regs;
        Sum sum;

        Fanin(::dspsim::ModuleName, size_t n, size_t changes)
            : sigs{"sigs", {n}}, regs{"regs", n, changes}, sum{"sum", n}
        {
            regs.clk.bind(clk);
            regs.in.bind(in);
            regs.q.bind(sigs);

            sum.d.bind(sigs);
            sum.out.bind(out);
        }
    };

    /*
        A balanced tree of two-input adders over n registers (n is a power of two), which all change on
        every clock edge. The change settles in log2(n) delta cycles, with half as many evaluations in each
        as in the one before.

            in -> regs -> leaves[n] -> n - 1 adders -> out
    */
    template <typename Regs, typename Node, typename T>
    DSPSIM_MODULE(Tree)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::SignalArray<T> leaves;
        // sums[k] is the output of node k. The root is node 0 and drives `out`, which leaves sums[0] unused.
        dspsim::SignalArray<T> sums;

        Regs regs;
        std::vector<std::unique_ptr<Node>> nodes;

        Tree(::dspsim::ModuleName, size_t n)
            : leaves{"leaves", {n}}, sums{"sums", {n - 1}}, regs{"regs", n, n}
        {
            regs.clk.bind(clk);
            regs.in.bind(in);
            regs.q.bind(leaves);

            // The nodes are numbered like a binary heap: the inputs of node k are the nodes 2k + 1 and
            // 2k + 2, and the numbers from n - 1 on are the leaves.
            auto net = [this, n](size_t k) -> dspsim::Signal<T> &
            {
                return k < n - 1 ? sums.flat(k) : leaves.flat(k - (n - 1));
            };
            for (size_t k = 0; k < n - 1; ++k)
            {
                Node &node = *nodes.emplace_back(std::make_unique<Node>("node" + std::to_string(k)));
                node.a.bind(net(2 * k + 1));
                node.b.bind(net(2 * k + 2));
                if (k == 0)
                {
                    node.out.bind(out);
                }
                else
                {
                    node.out.bind(net(k));
                }
            }
        }
    };

    /*
        Fan-out: n taps on the input, and the sum of their outputs, which makes the output depend on every
        tap. An input change makes all the taps runnable in the same delta cycle: one event with n
        subscribers that is not a clock edge. The clock is not used: the design is combinational.

            in -> n taps -> tapped[n] -> sum -> out
    */
    template <typename Tap, typename Sum, typename T>
    DSPSIM_MODULE(Fanout)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};
        dspsim::SignalArray<T> tapped;

        std::vector<std::unique_ptr<Tap>> taps;
        Sum sum;

        Fanout(::dspsim::ModuleName, size_t n) : tapped{"tapped", {n}}, sum{"sum", n}
        {
            for (size_t i = 0; i < n; ++i)
            {
                Tap &tap = *taps.emplace_back(std::make_unique<Tap>("tap" + std::to_string(i), static_cast<T>(i)));
                tap.d.bind(in);
                tap.q.bind(tapped.flat(i));
            }
            sum.d.bind(tapped);
            sum.out.bind(out);
        }
    };
}
