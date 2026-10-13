/*
    dspsim models for the structs benchmarks: pipelines of registers whose ports are structs, structs of
    arrays and arrays of structs. structs_sc.h has the SystemC counterparts, and structs_values.h what the
    designs compute.

    A design is a source, `stages` register stages and a sink in series:

        in -> source -> link 0 -> stage 0 -> link 1 -> ... -> link n -> sink -> out

    The source is combinational and the stages and the sink are clocked. The modules are the same for every
    kind of a design. A kind says what the ports of a link are, how a value is read from them and written to
    them, and how two modules are connected.

    dspsim has two ways to give a module a struct port, and both are here:

    - A PortStruct: a port and a signal per field, grouped and bound as a whole (the kinds "struct", "soa",
      "aos"). The fields can have different directions (the duplex design). It is the same simulation as the
      ports without the struct (the kinds "ports").
    - A port with the struct as its value (Input<BusValue>): one signal carries all fields, and a write
      compares and copies the whole value (the kinds "value", "soa_value", "aos_value").
*/
#pragma once
#include "structs_values.h"

#include <dspsim/dspsim.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace benchmarks
{
    /// The base of a group of ports that is not a struct: the fields are plain members.
    struct NoStruct
    {
        explicit NoStruct(const std::string &) {}

        template <typename P>
        void add(const std::string &, P &)
        {
        }
    };

    /// Bind an output and an input to a signal of their own.
    template <typename T>
    void connect_ports(dspsim::Output<T> &out, dspsim::Input<T> &in, const std::string &name)
    {
        auto signal = dspsim::Signal<T>::create(name);
        out.bind(*signal);
        in.bind(*signal);
    }

    /// Bind two struct ports to the signals that one of them creates.
    template <typename Out, typename In>
    void connect_structs(Out &out, In &in, const std::string &name)
    {
        auto link = out.signals(name);
        out.bind(link);
        in.bind(link);
    }

    /*
        The bus design. A kind has the port types In and Out, constructed from a name, connect(), read(in)
        and write(out, value). The ports of a bus are the same members whether they are a struct or not.
    */
    template <typename Base, template <typename> class Port>
    struct IqPorts : Base
    {
        Port<int16_t> re;
        Port<int16_t> im;

        explicit IqPorts(const std::string &name) : Base(name), re(name + ".re"), im(name + ".im")
        {
            this->add("re", re);
            this->add("im", im);
        }
    };

    template <typename Base, template <typename> class Port>
    struct BusPorts : Base
    {
        Port<uint32_t> data;
        Port<uint8_t> tag;
        IqPorts<Base, Port> iq;

        explicit BusPorts(const std::string &name)
            : Base(name), data(name + ".data"), tag(name + ".tag"), iq(name + ".iq")
        {
            this->add("data", data);
            this->add("tag", tag);
            this->add("iq", iq);
        }
    };

    /// The fields of a bus, read from a port per field and written to them. P has the members of BusPorts.
    template <typename P>
    BusValue read_fields(const P &bus)
    {
        return {bus.data.read(), bus.tag.read(), {bus.iq.re.read(), bus.iq.im.read()}};
    }

    template <typename P>
    void write_fields(P &bus, const BusValue &value)
    {
        bus.data.write(value.data);
        bus.tag.write(value.tag);
        bus.iq.re.write(value.iq.re);
        bus.iq.im.write(value.iq.im);
    }

    /// Common to the two kinds with a port per field. Base says whether the ports are a struct.
    template <typename Base>
    struct BusAccess
    {
        using In = BusPorts<Base, dspsim::Input>;
        using Out = BusPorts<Base, dspsim::Output>;

        static BusValue read(const In &bus)
        {
            return read_fields(bus);
        }

        static void write(Out &bus, const BusValue &value)
        {
            write_fields(bus, value);
        }
    };

    /// Bus kind "ports": a port per field, each bound to a signal of its own.
    struct BusFields : BusAccess<NoStruct>
    {
        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_ports(out.data, in.data, name + ".data");
            connect_ports(out.tag, in.tag, name + ".tag");
            connect_ports(out.iq.re, in.iq.re, name + ".iq.re");
            connect_ports(out.iq.im, in.iq.im, name + ".iq.im");
        }
    };

    /// Bus kind "struct": the ports are a PortStruct, bound as a whole to the signals that it creates.
    struct BusStruct : BusAccess<dspsim::PortStruct>
    {
        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_structs(out, in, name);
        }
    };

    /// Bus kind "value": one port and one signal, with the struct as the value.
    struct BusSignal
    {
        using In = dspsim::Input<BusValue>;
        using Out = dspsim::Output<BusValue>;

        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_ports(out, in, name);
        }

        static const BusValue &read(const In &bus)
        {
            return bus.read();
        }

        static void write(Out &bus, const BusValue &value)
        {
            bus.write(value);
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(BusSource)
    {
    public:
        dspsim::Input<uint32_t> in{"in"};
        typename Kind::Out m{"m"};

        DSPSIM_CTOR(BusSource)
        {
            DSPSIM_METHOD(eval)
                ->always(in);
        }

        void eval()
        {
            Kind::write(m, bus_source(in.read()));
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(BusStage)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        typename Kind::In s{"s"};
        typename Kind::Out m{"m"};

        DSPSIM_CTOR(BusStage)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            Kind::write(m, bus_stage(Kind::read(s)));
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(BusSink)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        typename Kind::In s{"s"};
        dspsim::Output<uint32_t> out{"out"};

        DSPSIM_CTOR(BusSink)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            out.write(bus_sink(Kind::read(s)));
        }
    };

    /*
        The duplex design: the bus, and a field in the other direction. A link carries the bus from the module
        that drives it to the one that reads it, and an acknowledgement back, like an interface with modports
        in SystemVerilog. Every stage registers both. Out is the side of a link that drives the bus, In the
        side that reads it; read() and write() are those of the bus.
    */
    /// One side of a link. Fwd is the direction of the bus on that side, Bwd that of the acknowledgement.
    template <typename Base, template <typename> class Fwd, template <typename> class Bwd>
    struct DuplexPorts : Base
    {
        Fwd<uint32_t> data;
        Fwd<uint8_t> tag;
        IqPorts<Base, Fwd> iq;
        Bwd<uint16_t> ack;

        explicit DuplexPorts(const std::string &name)
            : Base(name), data(name + ".data"), tag(name + ".tag"), iq(name + ".iq"), ack(name + ".ack")
        {
            this->add("data", data);
            this->add("tag", tag);
            this->add("iq", iq);
            this->add("ack", ack);
        }
    };

    /// Common to the duplex kinds. Base says whether the ports of a side are a struct.
    template <typename Base>
    struct DuplexAccess
    {
        using In = DuplexPorts<Base, dspsim::Input, dspsim::Output>;
        using Out = DuplexPorts<Base, dspsim::Output, dspsim::Input>;

        static BusValue read(const In &bus)
        {
            return read_fields(bus);
        }

        static void write(Out &bus, const BusValue &value)
        {
            write_fields(bus, value);
        }
    };

    /// Duplex kind "ports": a port per field, each bound to a signal of its own.
    struct DuplexFields : DuplexAccess<NoStruct>
    {
        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_ports(out.data, in.data, name + ".data");
            connect_ports(out.tag, in.tag, name + ".tag");
            connect_ports(out.iq.re, in.iq.re, name + ".iq.re");
            connect_ports(out.iq.im, in.iq.im, name + ".iq.im");
            connect_ports(in.ack, out.ack, name + ".ack");
        }
    };

    /// Duplex kind "struct": each side is one PortStruct with fields in both directions, bound as a whole.
    struct DuplexStruct : DuplexAccess<dspsim::PortStruct>
    {
        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_structs(out, in, name);
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(DuplexSource)
    {
    public:
        dspsim::Input<uint32_t> in{"in"};
        typename Kind::Out m{"m"};

        DSPSIM_CTOR(DuplexSource)
        {
            DSPSIM_METHOD(eval)
                ->always(in, m.ack);
        }

        void eval()
        {
            Kind::write(m, duplex_source(in.read(), m.ack.read()));
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(DuplexStage)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        typename Kind::In s{"s"};
        typename Kind::Out m{"m"};

        DSPSIM_CTOR(DuplexStage)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            const BusValue value = Kind::read(s);
            Kind::write(m, bus_stage(value));
            s.ack.write(ack_stage(m.ack.read(), value));
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(DuplexSink)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        typename Kind::In s{"s"};
        dspsim::Output<uint32_t> out{"out"};

        DSPSIM_CTOR(DuplexSink)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            const BusValue value = Kind::read(s);
            out.write(bus_sink(value));
            s.ack.write(ack_sink(value));
        }
    };

    /*
        The arrays design: every link carries K elements with the fields data and gain.

        A kind has the port types In and Out, constructed from a name, connect(), and the three processes
        source(out, value), stage(in, out) and sink(in). A kind that reads and writes element by element has
        read(in, i) and write(out, i, value) and takes the processes from Elementwise.
    */
    /// The elements as an array per field. Base says whether the two arrays are a struct.
    template <typename Base, template <typename> class PortArray, int K>
    struct SoaPorts : Base
    {
        PortArray<uint32_t> data;
        PortArray<uint8_t> gain;

        explicit SoaPorts(const std::string &name) : Base(name), data(name + ".data", {K}), gain(name + ".gain", {K})
        {
            this->add("data", data);
            this->add("gain", gain);
        }
    };

    /// Common to the two kinds with an array per field.
    template <typename Base, int K>
    struct SoaAccess
    {
        using In = SoaPorts<Base, dspsim::InputArray, K>;
        using Out = SoaPorts<Base, dspsim::OutputArray, K>;

        static ElemValue read(const In &in, std::size_t i)
        {
            return {in.data.flat(i).read(), in.gain.flat(i).read()};
        }

        static void write(Out &out, std::size_t i, const ElemValue &value)
        {
            out.data.flat(i).write(value.data);
            out.gain.flat(i).write(value.gain);
        }
    };

    /// Arrays kind "ports": a port array per field, each bound to a signal array of its own.
    template <int K>
    struct ArrayFields : SoaAccess<NoStruct, K>, Elementwise<ArrayFields<K>, K>
    {
        using typename SoaAccess<NoStruct, K>::In;
        using typename SoaAccess<NoStruct, K>::Out;

        static void connect(Out &out, In &in, const std::string &name)
        {
            auto data = dspsim::SignalArray<uint32_t>::create(name + ".data", {K});
            out.data.bind(*data);
            in.data.bind(*data);
            auto gain = dspsim::SignalArray<uint8_t>::create(name + ".gain", {K});
            out.gain.bind(*gain);
            in.gain.bind(*gain);
        }
    };

    /// Arrays kind "soa": a struct of arrays, a PortStruct whose fields are port arrays.
    template <int K>
    struct ArraySoa : SoaAccess<dspsim::PortStruct, K>, Elementwise<ArraySoa<K>, K>
    {
        using typename SoaAccess<dspsim::PortStruct, K>::In;
        using typename SoaAccess<dspsim::PortStruct, K>::Out;

        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_structs(out, in, name);
        }
    };

    /// Arrays kind "soa_value": one port and one signal, with a struct of arrays as the value.
    template <int K>
    struct ArraySoaValue
    {
        using In = dspsim::Input<SoaValue<K>>;
        using Out = dspsim::Output<SoaValue<K>>;

        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_ports(out, in, name);
        }

        static void source(Out &out, uint32_t value)
        {
            out.write(soa_source<K>(value));
        }

        static void stage(const In &in, Out &out)
        {
            out.write(soa_stage(in.read()));
        }

        static uint32_t sink(const In &in)
        {
            return soa_sink(in.read());
        }
    };

    /// One element of an array of structs.
    template <template <typename> class Port>
    struct ElemPorts : dspsim::PortStruct
    {
        Port<uint32_t> data;
        Port<uint8_t> gain;

        explicit ElemPorts(const std::string &name) : PortStruct(name), data(name + ".data"), gain(name + ".gain")
        {
            add("data", data);
            add("gain", gain);
        }
    };

    /// An array of K structs, constructed from its name like the other ports.
    template <template <typename> class Port, int K>
    struct AosPorts : dspsim::PortStructArrayOf<ElemPorts<Port>>
    {
        explicit AosPorts(const std::string &name) : dspsim::PortStructArrayOf<ElemPorts<Port>>(name, {K}) {}
    };

    /// Common to the kinds whose ports are an array of structs.
    template <int K>
    struct AosAccess
    {
        using In = AosPorts<dspsim::Input, K>;
        using Out = AosPorts<dspsim::Output, K>;

        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_structs(out, in, name);
        }
    };

    /// Arrays kind "aos": an array of structs. An element is the struct at a flat index.
    template <int K>
    struct ArrayAos : AosAccess<K>, Elementwise<ArrayAos<K>, K>
    {
        using typename AosAccess<K>::In;
        using typename AosAccess<K>::Out;

        static ElemValue read(const In &in, std::size_t i)
        {
            auto &element = in.flat(i);
            return {element.data.read(), element.gain.read()};
        }

        static void write(Out &out, std::size_t i, const ElemValue &value)
        {
            auto &element = out.flat(i);
            element.data.write(value.data);
            element.gain.write(value.gain);
        }
    };

    /// Arrays kind "aos_index": the same array of structs, with every access written as ports[{i}].field.
    template <int K>
    struct ArrayAosIndex : AosAccess<K>, Elementwise<ArrayAosIndex<K>, K>
    {
        using typename AosAccess<K>::In;
        using typename AosAccess<K>::Out;

        static ElemValue read(const In &in, std::size_t i)
        {
            return {in[{i}].data.read(), in[{i}].gain.read()};
        }

        static void write(Out &out, std::size_t i, const ElemValue &value)
        {
            out[{i}].data.write(value.data);
            out[{i}].gain.write(value.gain);
        }
    };

    /// Arrays kind "aos_view": an array of structs that is accessed by field, through the views of one field
    /// of every element, which the module keeps.
    template <int K>
    struct ArrayAosView : Elementwise<ArrayAosView<K>, K>
    {
        struct In
        {
            AosPorts<dspsim::Input, K> structs;
            dspsim::InputArrayView<uint32_t> data;
            dspsim::InputArrayView<uint8_t> gain;

            explicit In(const std::string &name)
                : structs(name), data(structs.template input_array<uint32_t>("data")), gain(structs.template input_array<uint8_t>("gain"))
            {
            }
        };

        struct Out
        {
            AosPorts<dspsim::Output, K> structs;
            dspsim::OutputArrayView<uint32_t> data;
            dspsim::OutputArrayView<uint8_t> gain;

            explicit Out(const std::string &name)
                : structs(name), data(structs.template output_array<uint32_t>("data")), gain(structs.template output_array<uint8_t>("gain"))
            {
            }
        };

        static void connect(Out &out, In &in, const std::string &name)
        {
            connect_structs(out.structs, in.structs, name);
        }

        static ElemValue read(const In &in, std::size_t i)
        {
            return {in.data.flat(i).read(), in.gain.flat(i).read()};
        }

        static void write(Out &out, std::size_t i, const ElemValue &value)
        {
            out.data.flat(i).write(value.data);
            out.gain.flat(i).write(value.gain);
        }
    };

    /// Arrays kind "aos_value": an array of K ports and of K signals, with the struct of an element as their
    /// value.
    template <int K>
    struct ArrayAosValue : Elementwise<ArrayAosValue<K>, K>
    {
        struct In : dspsim::InputArray<ElemValue>
        {
            explicit In(const std::string &name) : dspsim::InputArray<ElemValue>(name, {K}) {}
        };

        struct Out : dspsim::OutputArray<ElemValue>
        {
            explicit Out(const std::string &name) : dspsim::OutputArray<ElemValue>(name, {K}) {}
        };

        static void connect(Out &out, In &in, const std::string &name)
        {
            auto link = dspsim::SignalArray<ElemValue>::create(name, {K});
            out.bind(*link);
            in.bind(*link);
        }

        static const ElemValue &read(const In &in, std::size_t i)
        {
            return in.flat(i).read();
        }

        static void write(Out &out, std::size_t i, const ElemValue &value)
        {
            out.flat(i).write(value);
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(ArraySource)
    {
    public:
        dspsim::Input<uint32_t> in{"in"};
        typename Kind::Out m{"m"};

        DSPSIM_CTOR(ArraySource)
        {
            DSPSIM_METHOD(eval)
                ->always(in);
        }

        void eval()
        {
            Kind::source(m, in.read());
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(ArrayStage)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        typename Kind::In s{"s"};
        typename Kind::Out m{"m"};

        DSPSIM_CTOR(ArrayStage)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            Kind::stage(s, m);
        }
    };

    template <typename Kind>
    DSPSIM_MODULE(ArraySink)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        typename Kind::In s{"s"};
        dspsim::Output<uint32_t> out{"out"};

        DSPSIM_CTOR(ArraySink)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos())
                ->initialize(false);
        }

        void eval()
        {
            out.write(Kind::sink(s));
        }
    };

    /// A source, `stages` stages and a sink in series, connected the way the kind connects two modules.
    template <typename Kind, typename Source, typename Stage, typename Sink>
    DSPSIM_MODULE(StructPipeline)
    {
    public:
        dspsim::Input<uint8_t> clk{"clk"};
        dspsim::Input<uint32_t> in{"in"};
        dspsim::Output<uint32_t> out{"out"};

        Source source{"source"};
        std::vector<std::unique_ptr<Stage>> stages;
        Sink sink{"sink"};

        StructPipeline(::dspsim::ModuleName, int count)
        {
            source.in.bind(in);
            auto *last = &source.m;
            for (int i = 0; i < count; ++i)
            {
                stages.push_back(std::make_unique<Stage>("stage" + std::to_string(i)));
                Stage &stage = *stages.back();
                stage.clk.bind(clk);
                Kind::connect(*last, stage.s, "link" + std::to_string(i));
                last = &stage.m;
            }
            sink.clk.bind(clk);
            Kind::connect(*last, sink.s, "link" + std::to_string(count));
            sink.out.bind(out);
        }
    };

    template <typename Kind>
    using BusPipeline = StructPipeline<Kind, BusSource<Kind>, BusStage<Kind>, BusSink<Kind>>;

    template <typename Kind>
    using DuplexPipeline = StructPipeline<Kind, DuplexSource<Kind>, DuplexStage<Kind>, DuplexSink<Kind>>;

    template <typename Kind>
    using ArrayPipeline = StructPipeline<Kind, ArraySource<Kind>, ArrayStage<Kind>, ArraySink<Kind>>;
}
