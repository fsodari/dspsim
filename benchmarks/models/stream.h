/*
    dspsim models for the stream benchmark: a pipeline of register slices with a valid/ready handshake
    (AXI-Stream without the sideband signals), fed by a source and drained by a sink that both stall at
    random. stream_sc.h has the SystemC counterparts.

    This is what register-transfer level code looks like to the engine: every stage has a clocked process
    and a combinational process with more than one input, the stages are submodules connected by arrays of
    signals, and the ready signal runs backwards through the stages combinationally. A clock edge makes
    every clocked process runnable at once, and the delta cycles after it carry the ready changes back one
    stage at a time, as far as the stalled part of the pipeline reaches.
*/
#pragma once
#include <dspsim/dspsim.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    /*
        Register slice: one register for the data and the valid flag. It accepts a word when it is empty or
        when the next stage takes the word it holds.

        Method processes.
    */
    template <typename T>
    DSPSIM_MODULE(SliceMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> s_data{"s_data"};
        dspsim::Input<uint8_t> s_valid{"s_valid"};
        dspsim::Output<uint8_t> s_ready{"s_ready"};
        dspsim::Output<T> m_data{"m_data"};
        dspsim::Output<uint8_t> m_valid{"m_valid"};
        dspsim::Input<uint8_t> m_ready{"m_ready"};

        DSPSIM_CTOR(SliceMethod)
        {
            DSPSIM_METHOD(seq)
                ->always(clk.pos())
                ->initialize(false);
            DSPSIM_METHOD(comb)
                ->always(m_valid, m_ready);
        }

        void seq()
        {
            if (s_valid.read() && s_ready.read())
            {
                m_data.write(s_data.read());
                m_valid.write(1);
            }
            else if (m_ready.read())
            {
                m_valid.write(0);
            }
        }

        void comb()
        {
            s_ready.write(!m_valid.read() || m_ready.read());
        }
    };

    /// Register slice with coroutine processes (static sensitivity).
    template <typename T>
    DSPSIM_MODULE(SliceCoro)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> s_data{"s_data"};
        dspsim::Input<uint8_t> s_valid{"s_valid"};
        dspsim::Output<uint8_t> s_ready{"s_ready"};
        dspsim::Output<T> m_data{"m_data"};
        dspsim::Output<uint8_t> m_valid{"m_valid"};
        dspsim::Input<uint8_t> m_ready{"m_ready"};

        DSPSIM_CTOR(SliceCoro)
        {
            DSPSIM_CORO(seq)
                ->always(clk.pos());
            DSPSIM_CORO(comb)
                ->always(m_valid, m_ready);
        }

        dspsim::Task<> seq()
        {
            while (true)
            {
                co_await wait();
                if (s_valid.read() && s_ready.read())
                {
                    m_data.write(s_data.read());
                    m_valid.write(1);
                }
                else if (m_ready.read())
                {
                    m_valid.write(0);
                }
            }
        }

        dspsim::Task<> comb()
        {
            while (true)
            {
                s_ready.write(!m_valid.read() || m_ready.read());
                co_await wait();
            }
        }
    };

    /*
        Source: offers a new word in three of four clock cycles, and holds a word until it is accepted. The
        word and the decision to offer one come from the input, which the testbench changes every cycle.
    */
    template <typename T>
    DSPSIM_MODULE(SourceMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> m_data{"m_data"};
        dspsim::Output<uint8_t> m_valid{"m_valid"};
        dspsim::Input<uint8_t> m_ready{"m_ready"};

        DSPSIM_CTOR(SourceMethod)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            if (!m_valid.read() || m_ready.read())
            {
                const T word = in.read();
                m_data.write(word >> 4);
                m_valid.write((word & 3) != 0);
            }
        }
    };

    template <typename T>
    DSPSIM_MODULE(SourceCoro)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> m_data{"m_data"};
        dspsim::Output<uint8_t> m_valid{"m_valid"};
        dspsim::Input<uint8_t> m_ready{"m_ready"};

        DSPSIM_CTOR(SourceCoro)
        {
            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        dspsim::Task<> eval()
        {
            while (true)
            {
                co_await wait();
                if (!m_valid.read() || m_ready.read())
                {
                    const T word = in.read();
                    m_data.write(word >> 4);
                    m_valid.write((word & 3) != 0);
                }
            }
        }
    };

    /*
        Sink: ready in three of four clock cycles, decided by the input. The output is a running hash of the
        words it accepted, so it depends on every word and on their order.
    */
    template <typename T>
    DSPSIM_MODULE(SinkMethod)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Input<T> s_data{"s_data"};
        dspsim::Input<uint8_t> s_valid{"s_valid"};
        dspsim::Output<uint8_t> s_ready{"s_ready"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(SinkMethod)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            if (s_valid.read() && s_ready.read())
            {
                hash_ = hash_ * 31 + s_data.read();
                out.write(hash_);
            }
            s_ready.write(((in.read() >> 2) & 3) != 0);
        }

    private:
        T hash_ = 0;
    };

    template <typename T>
    DSPSIM_MODULE(SinkCoro)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Input<T> s_data{"s_data"};
        dspsim::Input<uint8_t> s_valid{"s_valid"};
        dspsim::Output<uint8_t> s_ready{"s_ready"};
        dspsim::Output<T> out{"out"};

        DSPSIM_CTOR(SinkCoro)
        {
            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        dspsim::Task<> eval()
        {
            T hash = 0;
            while (true)
            {
                co_await wait();
                if (s_valid.read() && s_ready.read())
                {
                    hash = hash * 31 + s_data.read();
                    out.write(hash);
                }
                s_ready.write(((in.read() >> 2) & 3) != 0);
            }
        }
    };

    /*
        A source, `stages` register slices and a sink in series. Link i carries the stream into slice i, and
        the last link carries it into the sink.
    */
    template <typename Source, typename Slice, typename Sink, typename T>
    DSPSIM_MODULE(Pipeline)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<T> in{"in"};
        dspsim::Output<T> out{"out"};

        dspsim::SignalArray<T> data;
        dspsim::SignalArray<uint8_t> valid;
        dspsim::SignalArray<uint8_t> ready;

        Source source{"source"};
        std::vector<std::unique_ptr<Slice>> slices;
        Sink sink{"sink"};

        Pipeline(::dspsim::ModuleName, int stages)
            : data("data", {static_cast<size_t>(stages) + 1}),
              valid("valid", {static_cast<size_t>(stages) + 1}),
              ready("ready", {static_cast<size_t>(stages) + 1})
        {
            const size_t last = static_cast<size_t>(stages);

            source.clk.bind(clk);
            source.in.bind(in);
            source.m_data.bind(data.flat(0));
            source.m_valid.bind(valid.flat(0));
            source.m_ready.bind(ready.flat(0));

            for (size_t i = 0; i < last; ++i)
            {
                slices.push_back(std::make_unique<Slice>("slice" + std::to_string(i)));
                Slice &slice = *slices.back();
                slice.clk.bind(clk);
                slice.s_data.bind(data.flat(i));
                slice.s_valid.bind(valid.flat(i));
                slice.s_ready.bind(ready.flat(i));
                slice.m_data.bind(data.flat(i + 1));
                slice.m_valid.bind(valid.flat(i + 1));
                slice.m_ready.bind(ready.flat(i + 1));
            }

            sink.clk.bind(clk);
            sink.in.bind(in);
            sink.s_data.bind(data.flat(last));
            sink.s_valid.bind(valid.flat(last));
            sink.s_ready.bind(ready.flat(last));
            sink.out.bind(out);
        }
    };
}
