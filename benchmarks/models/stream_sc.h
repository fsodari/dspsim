/*
    SystemC counterparts of stream.h. A method process is an SC_METHOD and a coroutine process an SC_THREAD.
*/
#pragma once
#include <systemc>

#include <cstddef>

namespace benchmarks
{
    template <typename T>
    SC_MODULE(SliceMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> s_data{"s_data"};
        sc_core::sc_in<bool> s_valid{"s_valid"};
        sc_core::sc_out<bool> s_ready{"s_ready"};
        sc_core::sc_out<T> m_data{"m_data"};
        sc_core::sc_out<bool> m_valid{"m_valid"};
        sc_core::sc_in<bool> m_ready{"m_ready"};

        SC_CTOR(SliceMethodSC)
        {
            SC_METHOD(seq);
            sensitive << clk.pos();
            dont_initialize();
            SC_METHOD(comb);
            sensitive << m_valid << m_ready;
        }

        void seq()
        {
            if (s_valid.read() && s_ready.read())
            {
                m_data.write(s_data.read());
                m_valid.write(true);
            }
            else if (m_ready.read())
            {
                m_valid.write(false);
            }
        }

        void comb()
        {
            s_ready.write(!m_valid.read() || m_ready.read());
        }
    };

    template <typename T>
    SC_MODULE(SliceThreadSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> s_data{"s_data"};
        sc_core::sc_in<bool> s_valid{"s_valid"};
        sc_core::sc_out<bool> s_ready{"s_ready"};
        sc_core::sc_out<T> m_data{"m_data"};
        sc_core::sc_out<bool> m_valid{"m_valid"};
        sc_core::sc_in<bool> m_ready{"m_ready"};

        SC_CTOR(SliceThreadSC)
        {
            SC_THREAD(seq);
            sensitive << clk.pos();
            SC_THREAD(comb);
            sensitive << m_valid << m_ready;
        }

        void seq()
        {
            while (true)
            {
                wait();
                if (s_valid.read() && s_ready.read())
                {
                    m_data.write(s_data.read());
                    m_valid.write(true);
                }
                else if (m_ready.read())
                {
                    m_valid.write(false);
                }
            }
        }

        void comb()
        {
            while (true)
            {
                s_ready.write(!m_valid.read() || m_ready.read());
                wait();
            }
        }
    };

    template <typename T>
    SC_MODULE(SourceMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> m_data{"m_data"};
        sc_core::sc_out<bool> m_valid{"m_valid"};
        sc_core::sc_in<bool> m_ready{"m_ready"};

        SC_CTOR(SourceMethodSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
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
    SC_MODULE(SourceThreadSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> m_data{"m_data"};
        sc_core::sc_out<bool> m_valid{"m_valid"};
        sc_core::sc_in<bool> m_ready{"m_ready"};

        SC_CTOR(SourceThreadSC)
        {
            SC_THREAD(eval);
            sensitive << clk.pos();
        }

        void eval()
        {
            while (true)
            {
                wait();
                if (!m_valid.read() || m_ready.read())
                {
                    const T word = in.read();
                    m_data.write(word >> 4);
                    m_valid.write((word & 3) != 0);
                }
            }
        }
    };

    template <typename T>
    SC_MODULE(SinkMethodSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_in<T> s_data{"s_data"};
        sc_core::sc_in<bool> s_valid{"s_valid"};
        sc_core::sc_out<bool> s_ready{"s_ready"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(SinkMethodSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
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
    SC_MODULE(SinkThreadSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_in<T> s_data{"s_data"};
        sc_core::sc_in<bool> s_valid{"s_valid"};
        sc_core::sc_out<bool> s_ready{"s_ready"};
        sc_core::sc_out<T> out{"out"};

        SC_CTOR(SinkThreadSC)
        {
            SC_THREAD(eval);
            sensitive << clk.pos();
        }

        void eval()
        {
            T hash = 0;
            while (true)
            {
                wait();
                if (s_valid.read() && s_ready.read())
                {
                    hash = hash * 31 + s_data.read();
                    out.write(hash);
                }
                s_ready.write(((in.read() >> 2) & 3) != 0);
            }
        }
    };

    /// A source, `stages` register slices and a sink in series. See Pipeline in stream.h.
    template <typename Source, typename Slice, typename Sink, typename T>
    class PipelineSC : public sc_core::sc_module
    {
    public:
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<T> in{"in"};
        sc_core::sc_out<T> out{"out"};

        sc_core::sc_vector<sc_core::sc_signal<T>> data;
        sc_core::sc_vector<sc_core::sc_signal<bool>> valid;
        sc_core::sc_vector<sc_core::sc_signal<bool>> ready;

        Source source{"source"};
        sc_core::sc_vector<Slice> slices;
        Sink sink{"sink"};

        PipelineSC(sc_core::sc_module_name name, int stages)
            : sc_core::sc_module(name),
              data("data", static_cast<size_t>(stages) + 1),
              valid("valid", static_cast<size_t>(stages) + 1),
              ready("ready", static_cast<size_t>(stages) + 1),
              slices("slice", static_cast<size_t>(stages))
        {
            const size_t last = static_cast<size_t>(stages);

            source.clk(clk);
            source.in(in);
            source.m_data(data[0]);
            source.m_valid(valid[0]);
            source.m_ready(ready[0]);

            for (size_t i = 0; i < last; ++i)
            {
                Slice &slice = slices[i];
                slice.clk(clk);
                slice.s_data(data[i]);
                slice.s_valid(valid[i]);
                slice.s_ready(ready[i]);
                slice.m_data(data[i + 1]);
                slice.m_valid(valid[i + 1]);
                slice.m_ready(ready[i + 1]);
            }

            sink.clk(clk);
            sink.in(in);
            sink.s_data(data[last]);
            sink.s_valid(valid[last]);
            sink.s_ready(ready[last]);
            sink.out(out);
        }
    };
}
