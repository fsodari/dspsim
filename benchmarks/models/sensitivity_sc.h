/*
    SystemC counterparts of sensitivity.h. A method process is an SC_METHOD and a coroutine process an
    SC_THREAD. An array of signals, ports or modules is an sc_vector.
*/
#pragma once
#include <systemc>

#include <cstddef>

namespace benchmarks
{
    /// The bank of registers as an SC_METHOD. See RegsMethod in sensitivity.h.
    template <typename T>
    SC_MODULE(RegsMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_vector<sc_core::sc_out<T>> q;

        RegsMethodSC(sc_core::sc_module_name, size_t n, size_t changes) : q("q", n), changes_(changes)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
        }

        void eval()
        {
            const T value = in.read();
            size_t index = value % q.size();
            for (size_t i = 0; i < changes_; ++i)
            {
                q[index].write(value + static_cast<T>(i));
                index = index + 1 == q.size() ? 0 : index + 1;
            }
        }

    private:
        size_t changes_;
    };

    /// The bank of registers as an SC_THREAD with static sensitivity.
    template <typename T>
    SC_MODULE(RegsThreadSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_vector<sc_core::sc_out<T>> q;

        RegsThreadSC(sc_core::sc_module_name, size_t n, size_t changes) : q("q", n), changes_(changes)
        {
            SC_THREAD(eval);
            sensitive << clk.pos();
        }

        void eval()
        {
            while (true)
            {
                wait();
                const T value = in.read();
                size_t index = value % q.size();
                for (size_t i = 0; i < changes_; ++i)
                {
                    q[index].write(value + static_cast<T>(i));
                    index = index + 1 == q.size() ? 0 : index + 1;
                }
            }
        }

    private:
        size_t changes_;
    };

    /// The sum of N inputs as an SC_METHOD with static sensitivity. See SumMethod in sensitivity.h.
    template <typename T>
    SC_MODULE(SumMethodSC)
    {
        sc_core::sc_vector<sc_core::sc_in<T>> d;
        sc_core::sc_out<T> out{"out"};

        SumMethodSC(sc_core::sc_module_name, size_t n) : d("d", n)
        {
            SC_METHOD(eval);
            for (auto &input : d)
            {
                sensitive << input;
            }
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

    /// SC_METHOD with dynamic sensitivity: next_trigger() with a list of the N events, on every evaluation.
    template <typename T>
    SC_MODULE(SumMethodDynamicSC)
    {
        sc_core::sc_vector<sc_core::sc_in<T>> d;
        sc_core::sc_out<T> out{"out"};

        SumMethodDynamicSC(sc_core::sc_module_name, size_t n) : d("d", n)
        {
            SC_METHOD(eval);
        }

        // The ports are bound now, so their events are the events of the signals.
        void end_of_elaboration() override
        {
            for (auto &input : d)
            {
                any_input_ |= input.value_changed_event();
            }
        }

        void eval()
        {
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
        sc_core::sc_event_or_list any_input_;
    };

    /// SC_THREAD with static sensitivity.
    template <typename T>
    SC_MODULE(SumThreadSC)
    {
        sc_core::sc_vector<sc_core::sc_in<T>> d;
        sc_core::sc_out<T> out{"out"};

        SumThreadSC(sc_core::sc_module_name, size_t n) : d("d", n)
        {
            SC_THREAD(eval);
            for (auto &input : d)
            {
                sensitive << input;
            }
        }

        void eval()
        {
            while (true)
            {
                T sum = ++evals_;
                for (auto &input : d)
                {
                    sum += input.read();
                }
                out.write(sum);
                wait();
            }
        }

    private:
        T evals_ = 0;
    };

    /// SC_THREAD with dynamic sensitivity: wait() with a list of the N events, on every evaluation.
    template <typename T>
    SC_MODULE(SumThreadDynamicSC)
    {
        sc_core::sc_vector<sc_core::sc_in<T>> d;
        sc_core::sc_out<T> out{"out"};

        SumThreadDynamicSC(sc_core::sc_module_name, size_t n) : d("d", n)
        {
            SC_THREAD(eval);
        }

        void eval()
        {
            // The list of events is made once, when the process starts.
            sc_core::sc_event_or_list any_input;
            for (auto &input : d)
            {
                any_input |= input.value_changed_event();
            }
            while (true)
            {
                T sum = ++evals_;
                for (auto &input : d)
                {
                    sum += input.read();
                }
                out.write(sum);
                wait(any_input);
            }
        }

    private:
        T evals_ = 0;
    };

    /// The two-input adder. See AddMethod in sensitivity.h.
    template <typename T>
    SC_MODULE(AddMethodSC)
    {
        sc_core::sc_in<T> a{"a"};
        sc_core::sc_in<T> b{"b"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(AddMethodSC)
        {
            SC_METHOD(eval);
            sensitive << a << b;
        }

        void eval()
        {
            out.write(a.read() + b.read());
        }
    };

    template <typename T>
    SC_MODULE(AddThreadSC)
    {
        sc_core::sc_in<T> a{"a"};
        sc_core::sc_in<T> b{"b"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(AddThreadSC)
        {
            SC_THREAD(eval);
            sensitive << a << b;
        }

        void eval()
        {
            while (true)
            {
                out.write(a.read() + b.read());
                wait();
            }
        }
    };

    /// The tap on a shared signal. See TapMethod in sensitivity.h.
    template <typename T>
    SC_MODULE(TapMethodSC)
    {
        sc_core::sc_in<T> d{"d"};
        sc_core::sc_out<T> q{"q"};

        TapMethodSC(sc_core::sc_module_name, T offset) : offset_(offset)
        {
            SC_METHOD(eval);
            sensitive << d;
        }

        void eval()
        {
            q.write(d.read() + offset_);
        }

    private:
        T offset_;
    };

    template <typename T>
    SC_MODULE(TapThreadSC)
    {
        sc_core::sc_in<T> d{"d"};
        sc_core::sc_out<T> q{"q"};

        TapThreadSC(sc_core::sc_module_name, T offset) : offset_(offset)
        {
            SC_THREAD(eval);
            sensitive << d;
        }

        void eval()
        {
            while (true)
            {
                q.write(d.read() + offset_);
                wait();
            }
        }

    private:
        T offset_;
    };

    /// The sum of n registers, of which `changes` change on every clock edge. See Fanin in sensitivity.h.
    template <typename Regs, typename Sum, typename T>
    SC_MODULE(FaninSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_vector<sc_core::sc_signal<T>> sigs;

        Regs regs;
        Sum sum;

        FaninSC(sc_core::sc_module_name, size_t n, size_t changes)
            : sigs("sigs", n), regs("regs", n, changes), sum("sum", n)
        {
            regs.clk.bind(clk);
            regs.in.bind(in);
            regs.q.bind(sigs);

            sum.d.bind(sigs);
            sum.out.bind(out);
        }
    };

    /// A balanced tree of two-input adders over n registers. See Tree in sensitivity.h.
    template <typename Regs, typename Node, typename T>
    SC_MODULE(TreeSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_vector<sc_core::sc_signal<T>> leaves;
        // sums[k] is the output of node k. The root is node 0 and drives `out`, which leaves sums[0] unused.
        sc_core::sc_vector<sc_core::sc_signal<T>> sums;

        Regs regs;
        sc_core::sc_vector<Node> nodes;

        TreeSC(sc_core::sc_module_name, size_t n)
            : leaves("leaves", n), sums("sums", n - 1), regs("regs", n, n), nodes("nodes", n - 1)
        {
            regs.clk.bind(clk);
            regs.in.bind(in);
            regs.q.bind(leaves);

            auto net = [this, n](size_t k) -> sc_core::sc_signal<T> &
            {
                return k < n - 1 ? sums[k] : leaves[k - (n - 1)];
            };
            for (size_t k = 0; k < n - 1; ++k)
            {
                Node &node = nodes[k];
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

    /// n taps on the input, and the sum of their outputs. See Fanout in sensitivity.h.
    template <typename Tap, typename Sum, typename T>
    SC_MODULE(FanoutSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};
        sc_core::sc_vector<sc_core::sc_signal<T>> tapped;

        sc_core::sc_vector<Tap> taps;
        Sum sum;

        FanoutSC(sc_core::sc_module_name, size_t n) : tapped("tapped", n), taps("taps"), sum("sum", n)
        {
            taps.init(n, [](const char *name, size_t i)
                      { return new Tap(name, static_cast<T>(i)); });
            for (size_t i = 0; i < n; ++i)
            {
                taps[i].d.bind(in);
                taps[i].q.bind(tapped[i]);
            }
            sum.d.bind(tapped);
            sum.out.bind(out);
        }
    };
}
