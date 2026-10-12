/*
    SystemC counterparts of structs.h.

    SystemC has two ways to give a module a struct port, and both are here. A group of ports, one per field,
    is what a dspsim struct port is too: the kinds "ports". A single port whose value type is the struct
    (sc_in<BusValue>) is the other: one signal carries all fields, and a write compares and copies the whole
    value. The kinds "struct", "soa" and "aos" use that one.
*/
#pragma once
#include "structs_values.h"

#include <systemc>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    // A value type of an sc_signal needs sc_trace(). The benchmarks do not trace.
    inline void sc_trace(sc_core::sc_trace_file *, const BusValue &, const std::string &) {}
    inline void sc_trace(sc_core::sc_trace_file *, const ElemValue &, const std::string &) {}
    template <int K>
    void sc_trace(sc_core::sc_trace_file *, const SoaValue<K> &, const std::string &)
    {
    }

    /*
        The bus design. A kind has the port types In and Out, constructed from a name, the type Link of what
        connects them, constructed from a name, bind(port, link), read(in) and write(out, value).
    */
    /// Bus kind "ports": a port and a signal per field.
    struct BusFieldsSC
    {
        template <template <typename> class Port>
        struct Ports
        {
            Port<uint32_t> data;
            Port<uint8_t> tag;
            Port<int16_t> re;
            Port<int16_t> im;

            explicit Ports(const std::string &name)
                : data((name + "_data").c_str()), tag((name + "_tag").c_str()), re((name + "_iq_re").c_str()), im((name + "_iq_im").c_str())
            {
            }
        };
        using In = Ports<sc_core::sc_in>;
        using Out = Ports<sc_core::sc_out>;
        using Link = Ports<sc_core::sc_signal>;

        template <template <typename> class Port>
        static void bind(Ports<Port> &ports, Link &link)
        {
            ports.data(link.data);
            ports.tag(link.tag);
            ports.re(link.re);
            ports.im(link.im);
        }

        static BusValue read(const In &in)
        {
            return {in.data.read(), in.tag.read(), {in.re.read(), in.im.read()}};
        }

        static void write(Out &out, const BusValue &value)
        {
            out.data.write(value.data);
            out.tag.write(value.tag);
            out.re.write(value.iq.re);
            out.im.write(value.iq.im);
        }
    };

    /// Bus kind "struct": one port and one signal, with the struct as the value.
    struct BusValueSC
    {
        using In = sc_core::sc_in<BusValue>;
        using Out = sc_core::sc_out<BusValue>;
        using Link = sc_core::sc_signal<BusValue>;

        template <typename Port>
        static void bind(Port &port, Link &link)
        {
            port(link);
        }

        static const BusValue &read(const In &in)
        {
            return in.read();
        }

        static void write(Out &out, const BusValue &value)
        {
            out.write(value);
        }
    };

    template <typename Kind>
    SC_MODULE(BusSourceSC)
    {
        sc_core::sc_in<uint32_t> in{"in"};
        typename Kind::Out m{"m"};

        SC_CTOR(BusSourceSC)
        {
            SC_METHOD(eval);
            sensitive << in;
        }

        void eval()
        {
            Kind::write(m, bus_source(in.read()));
        }
    };

    template <typename Kind>
    SC_MODULE(BusStageSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        typename Kind::In s{"s"};
        typename Kind::Out m{"m"};

        SC_CTOR(BusStageSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
        }

        void eval()
        {
            Kind::write(m, bus_stage(Kind::read(s)));
        }
    };

    template <typename Kind>
    SC_MODULE(BusSinkSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        typename Kind::In s{"s"};
        sc_core::sc_out<uint32_t> out{"out"};

        SC_CTOR(BusSinkSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
        }

        void eval()
        {
            out.write(bus_sink(Kind::read(s)));
        }
    };

    /*
        The arrays design. A kind has In, Out, Link and bind() like a bus kind, and the three processes:
        source(out, value), stage(in, out) and sink(in).
    */
    /// The processes of a kind that reads and writes element by element: read(in, i), write(out, i, value).
    template <typename Kind, int K>
    struct ElementwiseSC
    {
        template <typename Out>
        static void source(Out &out, uint32_t value)
        {
            for (std::size_t i = 0; i < K; ++i)
            {
                Kind::write(out, i, array_source(value, i));
            }
        }

        template <typename In, typename Out>
        static void stage(const In &in, Out &out)
        {
            for (std::size_t i = 0; i < K; ++i)
            {
                Kind::write(out, i, array_stage(Kind::read(in, i)));
            }
        }

        template <typename In>
        static uint32_t sink(const In &in)
        {
            uint32_t hash = 0;
            for (std::size_t i = 0; i < K; ++i)
            {
                hash = array_sink(hash, Kind::read(in, i));
            }
            return hash;
        }
    };

    /// Arrays kind "ports": a vector of ports and a vector of signals per field.
    template <int K>
    struct ArrayFieldsSC : ElementwiseSC<ArrayFieldsSC<K>, K>
    {
        template <template <typename> class Port>
        struct Ports
        {
            sc_core::sc_vector<Port<uint32_t>> data;
            sc_core::sc_vector<Port<uint8_t>> gain;

            explicit Ports(const std::string &name) : data((name + "_data").c_str(), K), gain((name + "_gain").c_str(), K) {}
        };
        using In = Ports<sc_core::sc_in>;
        using Out = Ports<sc_core::sc_out>;
        using Link = Ports<sc_core::sc_signal>;

        template <template <typename> class Port>
        static void bind(Ports<Port> &ports, Link &link)
        {
            ports.data(link.data);
            ports.gain(link.gain);
        }

        static ElemValue read(const In &in, std::size_t i)
        {
            return {in.data[i].read(), in.gain[i].read()};
        }

        static void write(Out &out, std::size_t i, const ElemValue &value)
        {
            out.data[i].write(value.data);
            out.gain[i].write(value.gain);
        }
    };

    /// Arrays kind "soa": one port and one signal, with a struct of arrays as the value.
    template <int K>
    struct ArraySoaSC
    {
        using In = sc_core::sc_in<SoaValue<K>>;
        using Out = sc_core::sc_out<SoaValue<K>>;
        using Link = sc_core::sc_signal<SoaValue<K>>;

        template <typename Port>
        static void bind(Port &port, Link &link)
        {
            port(link);
        }

        static void source(Out &out, uint32_t value)
        {
            SoaValue<K> next;
            for (std::size_t i = 0; i < K; ++i)
            {
                set(next, i, array_source(value, i));
            }
            out.write(next);
        }

        static void stage(const In &in, Out &out)
        {
            const SoaValue<K> &current = in.read();
            SoaValue<K> next;
            for (std::size_t i = 0; i < K; ++i)
            {
                set(next, i, array_stage({current.data[i], current.gain[i]}));
            }
            out.write(next);
        }

        static uint32_t sink(const In &in)
        {
            const SoaValue<K> &current = in.read();
            uint32_t hash = 0;
            for (std::size_t i = 0; i < K; ++i)
            {
                hash = array_sink(hash, {current.data[i], current.gain[i]});
            }
            return hash;
        }

    private:
        static void set(SoaValue<K> &value, std::size_t i, const ElemValue &element)
        {
            value.data[i] = element.data;
            value.gain[i] = element.gain;
        }
    };

    /// Arrays kind "aos": a vector of ports and a vector of signals, with the struct of an element as the value.
    template <int K>
    struct ArrayAosSC : ElementwiseSC<ArrayAosSC<K>, K>
    {
        template <typename Port>
        struct Vector : sc_core::sc_vector<Port>
        {
            explicit Vector(const char *name) : sc_core::sc_vector<Port>(name, K) {}
        };
        using In = Vector<sc_core::sc_in<ElemValue>>;
        using Out = Vector<sc_core::sc_out<ElemValue>>;
        using Link = Vector<sc_core::sc_signal<ElemValue>>;

        template <typename Port>
        static void bind(Vector<Port> &ports, Link &link)
        {
            ports(link);
        }

        static const ElemValue &read(const In &in, std::size_t i)
        {
            return in[i].read();
        }

        static void write(Out &out, std::size_t i, const ElemValue &value)
        {
            out[i].write(value);
        }
    };

    template <typename Kind>
    SC_MODULE(ArraySourceSC)
    {
        sc_core::sc_in<uint32_t> in{"in"};
        typename Kind::Out m{"m"};

        SC_CTOR(ArraySourceSC)
        {
            SC_METHOD(eval);
            sensitive << in;
        }

        void eval()
        {
            Kind::source(m, in.read());
        }
    };

    template <typename Kind>
    SC_MODULE(ArrayStageSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        typename Kind::In s{"s"};
        typename Kind::Out m{"m"};

        SC_CTOR(ArrayStageSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
        }

        void eval()
        {
            Kind::stage(s, m);
        }
    };

    template <typename Kind>
    SC_MODULE(ArraySinkSC)
    {
        sc_core::sc_in<bool> clk{"clk"};
        typename Kind::In s{"s"};
        sc_core::sc_out<uint32_t> out{"out"};

        SC_CTOR(ArraySinkSC)
        {
            SC_METHOD(eval);
            sensitive << clk.pos();
            dont_initialize();
        }

        void eval()
        {
            out.write(Kind::sink(s));
        }
    };

    /// A source, `stages` stages and a sink in series. See StructPipeline in structs.h.
    template <typename Kind, typename Source, typename Stage, typename Sink>
    class StructPipelineSC : public sc_core::sc_module
    {
    public:
        sc_core::sc_in<bool> clk{"clk"};
        sc_core::sc_in<uint32_t> in{"in"};
        sc_core::sc_out<uint32_t> out{"out"};

        Source source{"source"};
        sc_core::sc_vector<Stage> stages;
        Sink sink{"sink"};
        std::vector<std::unique_ptr<typename Kind::Link>> links;

        StructPipelineSC(sc_core::sc_module_name name, int count)
            : sc_core::sc_module(name), stages("stage", static_cast<std::size_t>(count))
        {
            for (int i = 0; i <= count; ++i)
            {
                links.push_back(std::make_unique<typename Kind::Link>(("link" + std::to_string(i)).c_str()));
            }

            source.in(in);
            Kind::bind(source.m, *links.front());
            for (std::size_t i = 0; i < stages.size(); ++i)
            {
                stages[i].clk(clk);
                Kind::bind(stages[i].s, *links[i]);
                Kind::bind(stages[i].m, *links[i + 1]);
            }
            sink.clk(clk);
            Kind::bind(sink.s, *links.back());
            sink.out(out);
        }
    };

    template <typename Kind>
    using BusPipelineSC = StructPipelineSC<Kind, BusSourceSC<Kind>, BusStageSC<Kind>, BusSinkSC<Kind>>;

    template <typename Kind>
    using ArrayPipelineSC = StructPipelineSC<Kind, ArraySourceSC<Kind>, ArrayStageSC<Kind>, ArraySinkSC<Kind>>;
}
