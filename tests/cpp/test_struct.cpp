#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace dspsim;
using Catch::Matchers::ContainsSubstring;

namespace
{
    // A complex sample: the nested struct of the buses below.
    template <template <typename> class Port>
    struct IqPorts : PortStruct
    {
        Port<int8_t> re;
        Port<int8_t> im;

        IqPorts(const std::string &name) : PortStruct(name), re(name + ".re"), im(name + ".im")
        {
            add("re", re);
            add("im", im);
        }
    };

    template <template <typename> class Port, template <typename> class PortArray>
    struct BusPorts : PortStruct
    {
        Port<int16_t> data;
        Port<uint8_t> valid;
        PortArray<uint8_t> taps;
        IqPorts<Port> iq;

        BusPorts(const std::string &name)
            : PortStruct(name), data(name + ".data", 12), valid(name + ".valid", 1), taps(name + ".taps", {4}, 6), iq(name + ".iq")
        {
            add("data", data);
            add("valid", valid);
            add("taps", taps);
            add("iq", iq);
        }
    };
    using BusIn = BusPorts<Input, InputArray>;
    using BusOut = BusPorts<Output, OutputArray>;

    // Negates the data and the sample, reverses the taps.
    class StructStage : public Module
    {
    public:
        BusIn in{"in"};
        BusOut out{"out"};

        StructStage(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(eval)->always("*");
        }

        void eval()
        {
            out.data = -in.data.read();
            out.valid = in.valid.read();
            for (std::size_t i = 0; i < 4; ++i)
            {
                out.taps[{i}] = in.taps[{3 - i}].read();
            }
            out.iq.re = -in.iq.re.read();
            out.iq.im = -in.iq.im.read();
        }
    };

    // A stage inside a module with struct ports of its own.
    class StructWrapper : public Module
    {
    public:
        BusIn in{"in"};
        BusOut out{"out"};
        StructStage stage{"stage"};

        StructWrapper(ModuleName name) : Module(name)
        {
            stage.in.bind(in);
            stage.out.bind(out);
        }
    };
}

TEST_CASE("struct field types", "[struct]")
{
    // The type of a field is equal to the same type and to no other. Types are compared by name, because a
    // model's extension module and the framework each have their own std::type_info of a type.
    StructFieldType type = typeid(Signal<int16_t>);
    REQUIRE(type == typeid(Signal<int16_t>));
    REQUIRE(type == StructFieldType{typeid(Signal<int16_t>)});
    REQUIRE(type != typeid(Signal<uint16_t>));
    REQUIRE(type != typeid(SignalArray<int16_t>));
    REQUIRE(StructFieldType{typeid(PortStruct)} != typeid(PortStructArray));
}

TEST_CASE("signals of a port struct", "[struct]")
{
    auto ctx = Context::create();
    StructStage stage{"stage"};

    SignalStruct bus = stage.in.signals("bus");
    REQUIRE(bus.name() == "bus");
    REQUIRE(bus.fields().size() == 4);
    REQUIRE(bus.fields()[0].name == "data");
    REQUIRE(bus.fields()[3].name == "iq");

    // One signal per field, with the type, width and shape of the port.
    REQUIRE(bus.signal<int16_t>("data").name() == "bus.data");
    REQUIRE(bus.signal<int16_t>("data").width() == 12);
    REQUIRE(bus.signal<uint8_t>("valid").width() == 1);
    REQUIRE(bus.array<uint8_t>("taps").shape() == Shape{4});
    REQUIRE(bus.array<uint8_t>("taps")[{0}].width() == 6);
    REQUIRE(bus.nested("iq").signal<int8_t>("re").name() == "bus.iq.re");

    REQUIRE(bus.find("valid") != nullptr);
    REQUIRE(bus.find("ready") == nullptr);
    REQUIRE_THROWS_WITH(bus.signal<int16_t>("ready"), ContainsSubstring("no field 'ready'"));
    REQUIRE_THROWS_WITH(bus.signal<uint8_t>("data"), ContainsSubstring("another type"));
    REQUIRE_THROWS_WITH(bus.signal<uint8_t>("taps"), ContainsSubstring("another type"));

    // Ports by field name.
    REQUIRE(&stage.in.input<int16_t>("data") == &stage.in.data);
    REQUIRE(&stage.out.output_array<uint8_t>("taps") == &stage.out.taps);
    REQUIRE(&stage.in.nested("iq") == &stage.in.iq);
    REQUIRE_THROWS_WITH(stage.in.output<int16_t>("data"), ContainsSubstring("another type"));
}

TEST_CASE("whole struct bind", "[struct]")
{
    auto ctx = Context::create();
    StructStage a{"a"};
    StructStage b{"b"};

    auto in = a.in.signals("in");
    auto mid = a.out.signals("mid");
    auto out = b.out.signals("out");
    a.in.bind(in);
    a.out.bind(mid);
    b.in.bind(mid);
    b.out.bind(out);
    ctx->elaborate();

    in.signal<int16_t>("data") = -123;
    in.signal<uint8_t>("valid") = 1;
    for (std::size_t i = 0; i < 4; ++i)
    {
        in.array<uint8_t>("taps")[{i}] = static_cast<uint8_t>(10 + i);
    }
    in.nested("iq").signal<int8_t>("re") = 5;
    in.nested("iq").signal<int8_t>("im") = -6;
    ctx->run(1);

    REQUIRE(mid.signal<int16_t>("data").read() == 123);
    REQUIRE(mid.array<uint8_t>("taps")[{0}].read() == 13);
    REQUIRE(mid.nested("iq").signal<int8_t>("im").read() == 6);
    // Two stages undo each other.
    REQUIRE(out.signal<int16_t>("data").read() == -123);
    REQUIRE(out.signal<uint8_t>("valid").read() == 1);
    REQUIRE(out.array<uint8_t>("taps").read() == std::vector<uint8_t>{10, 11, 12, 13});
    REQUIRE(out.nested("iq").signal<int8_t>("re").read() == 5);
    REQUIRE(out.nested("iq").signal<int8_t>("im").read() == -6);
}

TEST_CASE("port struct bound to the port struct of a parent", "[struct]")
{
    auto ctx = Context::create();
    StructWrapper top{"top"};
    auto in = top.in.signals("in");
    auto out = top.out.signals("out");
    top.in.bind(in);
    top.out.bind(out);
    ctx->elaborate();

    in.signal<int16_t>("data") = 77;
    in.nested("iq").signal<int8_t>("re") = 9;
    in.array<uint8_t>("taps")[{3}] = 4;
    ctx->run(1);
    REQUIRE(out.signal<int16_t>("data").read() == -77);
    REQUIRE(out.nested("iq").signal<int8_t>("re").read() == -9);
    REQUIRE(out.array<uint8_t>("taps")[{0}].read() == 4);
}

TEST_CASE("signal struct from existing signals", "[struct]")
{
    auto ctx = Context::create();
    StructStage stage{"stage"};

    Signal<int16_t> data{"data", 12};
    Signal<uint8_t> valid{"valid", 1};
    SignalArray<uint8_t> taps{"taps", {4}, 6};
    Signal<int8_t> re{"re"}, im{"im"};

    SignalStruct iq{"iq"};
    iq.add("re", re).add("im", im);
    SignalStruct bus{"bus"};
    bus.add("data", data).add("valid", valid).add("taps", taps).add("iq", iq);
    REQUIRE_THROWS_WITH(bus.add("data", data), ContainsSubstring("already has a field 'data'"));
    REQUIRE(&bus.signal<int16_t>("data") == &data);
    // A copy refers to the same signals.
    SignalStruct copy = bus;
    REQUIRE(&copy.nested("iq").signal<int8_t>("im") == &im);

    auto out = stage.out.signals("out");
    stage.in.bind(bus);
    stage.out.bind(out);
    ctx->elaborate();

    data = 40;
    im = 3;
    ctx->run(1);
    REQUIRE(out.signal<int16_t>("data").read() == -40);
    REQUIRE(out.nested("iq").signal<int8_t>("im").read() == -3);
}

TEST_CASE("struct bind errors", "[struct]")
{
    auto ctx = Context::create();
    StructStage stage{"stage"};

    Signal<int16_t> data{"data"};
    Signal<int32_t> wrong{"wrong"};
    Signal<uint8_t> valid{"valid"};
    SignalArray<uint8_t> taps{"taps", {4}};
    Signal<int8_t> re{"re"}, im{"im"};

    // Every difference is reported, with the path of the field.
    SignalStruct iq{"iq"};
    iq.add("re", re).add("q", im);
    SignalStruct bus{"bus"};
    bus.add("data", wrong).add("taps", taps).add("iq", iq).add("extra", valid);
    try
    {
        stage.in.bind(bus);
        FAIL("bind did not throw");
    }
    catch (const std::invalid_argument &e)
    {
        std::string what = e.what();
        REQUIRE_THAT(what, ContainsSubstring("Cannot bind port struct 'in' to 'bus'"));
        REQUIRE_THAT(what, ContainsSubstring("field 'data' of the signals does not have the type of the port"));
        REQUIRE_THAT(what, ContainsSubstring("no field 'valid' in the signals"));
        REQUIRE_THAT(what, ContainsSubstring("no field 'iq.im' in the signals"));
        REQUIRE_THAT(what, ContainsSubstring("no field 'iq.q' in the ports"));
        REQUIRE_THAT(what, ContainsSubstring("no field 'extra' in the ports"));
    }

    // An input struct does not bind to an output struct.
    REQUIRE_THROWS_WITH(stage.in.bind(stage.out), ContainsSubstring("field 'data' of the other ports has another type"));

    // Nothing was bound by the failed attempts: the struct binds to matching signals.
    auto in = stage.in.signals("in");
    auto out = stage.out.signals("out");
    stage.in.bind(in);
    stage.out.bind(out);
    ctx->elaborate();
    in.signal<int16_t>("data") = 2;
    ctx->run(1);
    REQUIRE(out.signal<int16_t>("data").read() == -2);
}

namespace
{
    // An array of buses in and out, and a struct with an array of structs as a field.
    class StructArrayStage : public Module
    {
    public:
        struct Samples : PortStruct
        {
            PortStructArrayOf<IqPorts<Input>> iqs;
            Input<uint8_t> valid;

            Samples(const std::string &name) : PortStruct(name), iqs(name + ".iqs", {3}), valid(name + ".valid", 1)
            {
                add("iqs", iqs);
                add("valid", valid);
            }
        };

        PortStructArrayOf<BusIn> in{"in", {2, 3}};
        PortStructArrayOf<BusOut> out{"out", {2, 3}};
        Samples samples{"samples"};
        Output<int16_t> sum{"sum"};

        StructArrayStage(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(eval)->always("*");
        }

        void eval()
        {
            for (std::size_t i = 0; i < 2; ++i)
            {
                for (std::size_t j = 0; j < 3; ++j)
                {
                    out[{i, j}].data = in[{i, j}].data.read() + static_cast<int16_t>(3 * i + j);
                    out[{i, j}].valid = in[{i, j}].valid.read();
                    out[{i, j}].taps[{0}] = in[{i, j}].taps[{3}].read();
                    out[{i, j}].iq.re = -in[{i, j}].iq.re.read();
                }
            }
            int16_t total = 0;
            for (std::size_t i = 0; i < 3; ++i)
            {
                total += samples.iqs[{i}].re.read() * (samples.valid.read() ? 1 : 0);
            }
            sum = total;
        }
    };

    // The arrays of a stage inside a module with arrays of its own.
    class StructArrayWrapper : public Module
    {
    public:
        PortStructArrayOf<BusIn> in{"in", {2, 3}};
        PortStructArrayOf<BusOut> out{"out", {2, 3}};
        StructArrayStage stage{"stage"};

        StructArrayWrapper(ModuleName name) : Module(name)
        {
            stage.in.bind(in);
            stage.out.bind(out);
        }
    };
}

TEST_CASE("array of port structs", "[struct][array]")
{
    auto ctx = Context::create();
    StructArrayStage stage{"stage"};

    // The elements are structs, named after their place in the array.
    REQUIRE(stage.in.shape() == Shape{2, 3});
    REQUIRE(stage.in.size() == 6);
    REQUIRE(stage.in[{1, 2}].name() == "in[1][2]");
    REQUIRE(stage.in[{1, 2}].data.name() == "in[1][2].data");
    REQUIRE(stage.in[{1, 2}].iq.re.hier_name() == "root.stage.in[1][2].iq.re");
    REQUIRE(&stage.in.flat(5) == &stage.in[{1, 2}]);
    REQUIRE_THROWS_AS((stage.in[{2, 0}]), std::out_of_range);
    REQUIRE(stage.samples.iqs.shape() == Shape{3});
    REQUIRE(&stage.samples.struct_array("iqs") == &stage.samples.iqs);

    // A view of some of the elements.
    PortStructArray row = stage.in.slice({Slice::at(1)});
    REQUIRE(row.shape() == Shape{3});
    REQUIRE(&row[{2}] == &stage.in[{1, 2}]);

    // One field of every element, as a struct of arrays has it.
    InputArrayView<int16_t> data = stage.in.input_array<int16_t>("data");
    REQUIRE(data.shape() == Shape{2, 3});
    REQUIRE(&data[{1, 2}] == &stage.in[{1, 2}].data);
    // The dimensions of a field that is an array come last.
    InputArrayView<uint8_t> taps = stage.in.input_array<uint8_t>("taps");
    REQUIRE(taps.shape() == Shape{2, 3, 4});
    REQUIRE(&taps[{1, 2, 3}] == &stage.in[{1, 2}].taps[{3}]);
    PortStructArray iq = stage.in.nested("iq");
    REQUIRE(iq.shape() == Shape{2, 3});
    REQUIRE(&iq.input_array<int8_t>("re")[{0, 1}] == &stage.in[{0, 1}].iq.re);
    REQUIRE(stage.out.output_array<int16_t>("data").size() == 6);
    REQUIRE_THROWS_WITH(stage.in.input_array<int16_t>("ready"), ContainsSubstring("has no field 'ready'"));
    REQUIRE_THROWS_WITH(stage.in.output_array<int16_t>("data"), ContainsSubstring("has another type"));

    REQUIRE_THROWS_WITH((PortStructArray{"bad", {2, 2}, {&stage.in.flat(0)}}), ContainsSubstring("not what its shape holds"));
}

TEST_CASE("signals of an array of port structs", "[struct][array]")
{
    auto ctx = Context::create();
    StructArrayStage stage{"stage"};

    SignalStructArray in = stage.in.signals("in");
    SignalStructArray out = stage.out.signals("out");
    SignalStruct samples = stage.samples.signals("samples");
    Signal<int16_t> sum{"sum"};
    REQUIRE(in.name() == "in");
    REQUIRE(in.shape() == Shape{2, 3});
    REQUIRE(in[{1, 2}].name() == "in[1][2]");
    REQUIRE(in[{1, 2}].signal<int16_t>("data").name() == "in[1][2].data");
    REQUIRE(in[{1, 2}].signal<int16_t>("data").width() == 12);
    REQUIRE(samples.struct_array("iqs").shape() == Shape{3});
    REQUIRE(samples.struct_array("iqs")[{2}].signal<int8_t>("re").name() == "samples.iqs[2].re");

    stage.in.bind(in);
    stage.out.bind(out);
    stage.samples.bind(samples);
    stage.sum.bind(sum);
    ctx->elaborate();

    // Written element by element, and field by field.
    in[{1, 2}].signal<int16_t>("data") = 100;
    in[{0, 1}].array<uint8_t>("taps")[{3}] = 9;
    SignalArrayView<int8_t> re = in.nested("iq").array<int8_t>("re");
    REQUIRE(re.shape() == Shape{2, 3});
    re.write(std::vector<int8_t>{1, 2, 3, 4, 5, 6});
    samples.struct_array("iqs").array<int8_t>("re").write(std::vector<int8_t>{10, 20, 30});
    samples.signal<uint8_t>("valid") = 1;
    ctx->run(1);

    REQUIRE(out.array<int16_t>("data").read() == std::vector<int16_t>{0, 1, 2, 3, 4, 105});
    REQUIRE(out[{0, 1}].array<uint8_t>("taps")[{0}].read() == 9);
    REQUIRE(out.array<uint8_t>("taps").shape() == Shape{2, 3, 4});
    REQUIRE(out.nested("iq").array<int8_t>("re").read() == std::vector<int8_t>{-1, -2, -3, -4, -5, -6});
    REQUIRE(out[{1, 0}].nested("iq").signal<int8_t>("re").read() == -4);
    REQUIRE(sum.read() == 60);

    // A view refers to the same structs.
    SignalStructArray row = out.slice({Slice::at(1)});
    REQUIRE(row.shape() == Shape{3});
    REQUIRE(&row[{2}].signal<int16_t>("data") == &out[{1, 2}].signal<int16_t>("data"));
}

TEST_CASE("array of port structs bound to the array of a parent", "[struct][array]")
{
    auto ctx = Context::create();
    StructArrayWrapper top{"top"};
    auto in = top.in.signals("in");
    auto out = top.out.signals("out");
    auto samples = top.stage.samples.signals("samples");
    Signal<int16_t> sum{"sum"};
    top.in.bind(in);
    top.out.bind(out);
    top.stage.samples.bind(samples);
    top.stage.sum.bind(sum);
    ctx->elaborate();

    in[{1, 1}].signal<int16_t>("data") = 7;
    ctx->run(1);
    REQUIRE(out[{1, 1}].signal<int16_t>("data").read() == 11);
    REQUIRE(out[{0, 0}].signal<int16_t>("data").read() == 0);
}

TEST_CASE("struct array bind errors", "[struct][array]")
{
    auto ctx = Context::create();
    StructArrayStage stage{"stage"};

    // The same structs in another shape.
    SignalStructArray in = stage.in.signals("in");
    std::vector<SignalStruct> structs;
    for (auto &s : in)
    {
        structs.push_back(s);
    }
    SignalStructArray wrong_shape{"wrong_shape", {3, 2}, structs};
    REQUIRE_THROWS_WITH(stage.in.bind(wrong_shape), ContainsSubstring("has the shape (2, 3) in the ports and (3, 2) in the signals"));

    // An element with other fields: the error has the place of the element.
    Signal<int16_t> data{"data"};
    structs[5] = SignalStruct{"odd"}.add("data", data);
    SignalStructArray odd{"odd", {2, 3}, structs};
    REQUIRE_THROWS_WITH(stage.in.bind(odd), ContainsSubstring("no field '[1][2].valid' in the signals"));

    // An array of structs that is a field.
    SignalStruct samples = stage.samples.signals("samples");
    SignalStruct short_samples{"short"};
    short_samples.add("valid", samples.signal<uint8_t>("valid")).add("iqs", samples.struct_array("iqs").slice({Slice{0, 2}}));
    REQUIRE_THROWS_WITH(stage.samples.bind(short_samples), ContainsSubstring("array 'iqs' has the shape (3) in the ports and (2) in the signals"));

    // An array of inputs does not bind to an array of outputs.
    REQUIRE_THROWS_WITH(stage.in.bind(stage.out), ContainsSubstring("field '[0][0].data' of the other ports has another type"));

    // Nothing was bound by the failed attempts.
    auto out = stage.out.signals("out");
    Signal<int16_t> sum{"sum"};
    stage.in.bind(in);
    stage.out.bind(out);
    stage.samples.bind(samples);
    stage.sum.bind(sum);
    ctx->elaborate();
    in[{0, 2}].signal<int16_t>("data") = 5;
    ctx->run(1);
    REQUIRE(out[{0, 2}].signal<int16_t>("data").read() == 7);
}
