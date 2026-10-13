/*
    The description of a struct (DSPSIM_STRUCT), signals and ports without the type of their value, and the
    fields of a struct signal as signals of their own.
*/
#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace dspsim;
using Catch::Matchers::ContainsSubstring;

// The structs are in a namespace, as those of a model would be: the macro goes next to the struct.
namespace field_test
{
    enum class Mode : uint8_t
    {
        off,
        on
    };

    struct Iq
    {
        int16_t re = 0;
        int16_t im = 0;

        bool operator==(const Iq &) const = default;
    };
    DSPSIM_STRUCT(Iq, re, im)

    struct Bus
    {
        uint16_t tag = 0;
        bool valid = false;
        uint32_t data = 0;
        Iq iq;
        std::array<uint8_t, 3> taps{};
        std::array<std::array<Iq, 2>, 2> grid{};
        double gain = 0;
        Mode mode = Mode::off;

        bool operator==(const Bus &) const = default;
    };
    DSPSIM_STRUCT(Bus, tag, valid, data, iq, taps, grid, gain, mode)

    // No DSPSIM_STRUCT: a value for C++ only.
    struct Plain
    {
        int a = 0;

        bool operator==(const Plain &) const = default;
    };
}

namespace
{
    using field_test::Bus;
    using field_test::Iq;
    using field_test::Mode;
    using field_test::Plain;

    // A module with a port per field of a bus.
    class FieldPorts : public Module
    {
    public:
        Input<uint16_t> tag{"tag"};
        Input<int16_t> re{"re"};
        Output<uint32_t> data{"data"};
        Output<Iq> iq{"iq"};
        int evaluations = 0;

        FieldPorts(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(eval)->always(tag, re)->initialize(false);
        }

        void eval()
        {
            ++evaluations;
            data.write(static_cast<uint32_t>(tag.read() * 2));
            iq.write({static_cast<int16_t>(re.read() + 1), 7});
        }
    };

    class BusPorts : public Module
    {
    public:
        Input<Bus> in{"in"};
        Output<Bus> out{"out"};

        BusPorts(ModuleName name) : Module(name) {}
    };
}

TEST_CASE("the layout of a struct", "[value][layout]")
{
    const StructLayout &layout = dspsim_struct_layout(static_cast<const Bus *>(nullptr));
    REQUIRE(layout.name == "Bus");
    REQUIRE(layout.size == sizeof(Bus));

    std::vector<std::string> names;
    for (auto &f : layout.fields)
    {
        names.push_back(f.name);
    }
    REQUIRE(names == std::vector<std::string>{"tag", "valid", "data", "iq", "taps", "grid", "gain", "mode"});

    // The offsets are those of the struct, padding included.
    REQUIRE(layout.find("tag")->offset == offsetof(Bus, tag));
    REQUIRE(layout.find("valid")->offset == offsetof(Bus, valid));
    REQUIRE(layout.find("data")->offset == offsetof(Bus, data));
    REQUIRE(layout.find("data")->offset == 4);
    REQUIRE(layout.find("gain")->offset == offsetof(Bus, gain));
    REQUIRE(layout.find("nothing") == nullptr);

    using Kind = ValueType::Kind;
    REQUIRE(layout.find("tag")->type.kind == Kind::unsigned_integer);
    REQUIRE(layout.find("tag")->type.size == 2);
    REQUIRE(layout.find("tag")->type.shape.empty());
    REQUIRE(layout.find("valid")->type.kind == Kind::boolean);
    REQUIRE(layout.find("gain")->type.kind == Kind::floating);
    REQUIRE(layout.find("gain")->type.size == 8);
    // An enumeration is its underlying type.
    REQUIRE(layout.find("mode")->type.kind == Kind::unsigned_integer);
    REQUIRE(layout.find("mode")->type.size == 1);

    // A nested struct has its own layout.
    const ValueType &iq = layout.find("iq")->type;
    REQUIRE(iq.kind == Kind::structure);
    REQUIRE(iq.size == sizeof(Iq));
    REQUIRE(iq.layout->name == "Iq");
    REQUIRE(iq.layout->find("im")->offset == 2);
    REQUIRE(iq.layout->find("im")->type.kind == Kind::signed_integer);

    // Arrays have a shape, and the type of an element.
    REQUIRE(layout.find("taps")->type.shape == Shape{3});
    REQUIRE(layout.find("taps")->type.size == 1);
    REQUIRE(layout.find("grid")->type.shape == Shape{2, 2});
    REQUIRE(layout.find("grid")->type.kind == Kind::structure);
    REQUIRE(layout.find("grid")->type.size == sizeof(Iq));
}

TEST_CASE("the description of a value type", "[value][layout]")
{
    using Kind = ValueType::Kind;
    REQUIRE(value_type_of<uint8_t>()->kind == Kind::unsigned_integer);
    REQUIRE(value_type_of<int64_t>()->kind == Kind::signed_integer);
    REQUIRE(value_type_of<int64_t>()->size == 8);
    REQUIRE(value_type_of<float>()->kind == Kind::floating);
    REQUIRE(value_type_of<Mode>()->size == 1);
    REQUIRE(value_type_of<Bus>()->kind == Kind::structure);
    REQUIRE(value_type_of<Bus>()->layout->name == "Bus");
    REQUIRE(value_type_of<std::array<uint16_t, 4>>()->shape == Shape{4});
    REQUIRE(value_type_of<std::array<Iq, 4>>()->layout->name == "Iq");
    // A struct without a DSPSIM_STRUCT has none.
    REQUIRE(value_type_of<Plain>() == nullptr);
    REQUIRE(value_type_of<std::array<Plain, 2>>() == nullptr);

    auto ctx = Context::create();
    Signal<Bus> bus{"bus"};
    Signal<Plain> plain{"plain"};
    Signal<uint32_t> number{"number"};
    REQUIRE(bus.value_type() == value_type_of<Bus>());
    REQUIRE(plain.value_type() == nullptr);
    REQUIRE(number.value_type()->size == 4);
    REQUIRE(bus.value_typeid() == typeid(Bus));
}

TEST_CASE("a signal and a port without the type of their value", "[value][layout]")
{
    auto ctx = Context::create();
    BusPorts dut{"dut"};
    PortBase &in = dut.in;
    PortBase &out = dut.out;
    REQUIRE(in.value_type() == value_type_of<Bus>());
    REQUIRE_FALSE(in.is_output());
    REQUIRE(out.is_output());

    // A port creates the signal that it binds to.
    std::shared_ptr<SignalBase> a = in.create_signal("a");
    std::shared_ptr<SignalBase> b = out.create_signal("b");
    REQUIRE(a->name() == "a");
    REQUIRE(dynamic_cast<Signal<Bus> *>(a.get()) != nullptr);
    in.bind_any(*a);
    out.bind_any(*b);

    // Only to its own value type, and a port only to a port of its direction.
    Signal<Iq> iq{"iq"};
    Signal<uint32_t> number{"number"};
    BusPorts other{"other"};
    REQUIRE_THROWS_WITH(other.in.bind_any(iq), ContainsSubstring("another value type"));
    REQUIRE_THROWS_WITH(other.in.bind_any(number), ContainsSubstring("another value type"));
    REQUIRE_THROWS_WITH(other.in.bind_any(static_cast<PortBase &>(dut.out)), ContainsSubstring("an input binds to an input"));
    other.in.bind_any(*a);
    other.out.bind_any(*out.create_signal("c"));
    ctx->elaborate();

    // The value as bytes.
    Bus value;
    value.tag = 0x1234;
    value.data = 99;
    value.iq = {3, -4};
    value.taps = {1, 2, 3};
    a->write_data(&value);
    REQUIRE(static_cast<Signal<Bus> &>(*a).read().data == 0);
    ctx->run(1);
    Bus read;
    std::memcpy(&read, in.value_data(), sizeof(Bus));
    REQUIRE(read == value);

    // Bytes that are not aligned.
    unsigned char bytes[sizeof(Bus) + 1];
    value.data = 100;
    std::memcpy(bytes + 1, &value, sizeof(Bus));
    out.write_data(bytes + 1);
    ctx->run(1);
    REQUIRE(dut.out.read().data == 100);
    REQUIRE_THROWS_WITH(in.write_data(&value), ContainsSubstring("is an input"));
}

TEST_CASE("a field of a struct signal is a signal", "[value][field]")
{
    auto ctx = Context::create();
    Signal<Bus> bus{"bus"};
    Signal<uint32_t> data{"data"};
    Signal<Iq> iq_out{"iq_out"};
    FieldPorts dut{"dut"};

    Signal<uint16_t> &tag = bus.field(&Bus::tag);
    // A field of a field.
    Signal<int16_t> &re = bus.field(&Bus::iq).field(&Iq::re);
    REQUIRE(tag.name() == "bus.tag");
    REQUIRE(re.name() == "bus.iq.re");
    dut.tag.bind(tag);
    dut.re.bind(re);
    dut.data.bind(data);
    dut.iq.bind(iq_out);
    ctx->elaborate();

    // The field has its value in the delta cycle in which the struct changes.
    Bus value;
    value.tag = 21;
    value.iq = {5, 6};
    bus.write(value);
    ctx->run(1);
    REQUIRE(tag.read() == 21);
    REQUIRE(re.read() == 5);
    REQUIRE(dut.evaluations == 1);
    REQUIRE(data.read() == 42);
    REQUIRE(iq_out.read() == Iq{6, 7});

    // Its events only occur when the field changes.
    value.data = 1000;
    value.iq.im = 60;
    value.taps[1] = 9;
    bus.write(value);
    ctx->run(1);
    REQUIRE(dut.evaluations == 1);

    value.iq.re = -5;
    bus.write(value);
    ctx->run(1);
    REQUIRE(dut.evaluations == 2);
    REQUIRE(iq_out.read() == Iq{-4, 7});

    // init() of the struct sets the fields at once.
    value.tag = 3;
    bus.init(value);
    REQUIRE(tag.read() == 3);
    REQUIRE_THROWS_AS(tag.init(4), std::logic_error);
}

TEST_CASE("a write to a field changes that field of the struct", "[value][field]")
{
    auto ctx = Context::create();
    Signal<Bus> bus{"bus"};
    Signal<uint16_t> tag_in{"tag_in"};
    Signal<int16_t> re_in{"re_in"};
    FieldPorts dut{"dut"};
    dut.tag.bind(tag_in);
    dut.re.bind(re_in);
    // The outputs of the module are fields of the bus: a number and a nested struct.
    dut.data.bind(bus.field(&Bus::data));
    dut.iq.bind(bus.field(&Bus::iq));
    // An element of an array.
    Signal<uint8_t> &tap = bus.field(&Bus::taps).element(1);
    REQUIRE(tap.name() == "bus.taps[1]");
    REQUIRE_THROWS_AS(bus.field(&Bus::taps).element(3), std::out_of_range);
    int changes = 0;
    ctx->register_process_func([&] { ++changes; }, "changes")->always(bus.change())->initialize(false);
    ctx->elaborate();

    Bus value;
    value.tag = 77;
    value.gain = 0.5;
    bus.write(value);
    ctx->run(1);
    REQUIRE(changes == 1);

    // The module writes two fields; the others keep their value.
    tag_in.write(10);
    re_in.write(4);
    ctx->run(1);
    REQUIRE(changes == 2);
    REQUIRE(bus.read().data == 20);
    REQUIRE(bus.read().iq == Iq{5, 7});
    REQUIRE(bus.read().tag == 77);
    REQUIRE(bus.read().gain == 0.5);

    // Writes to fields and to the whole struct in one evaluation phase combine: the last write of a field wins.
    value.tag = 78;
    value.data = 1;
    bus.write(value);
    tap.write(200);
    ctx->run(1);
    REQUIRE(changes == 3);
    REQUIRE(bus.read().tag == 78);
    REQUIRE(bus.read().data == 1);
    REQUIRE(bus.read().iq == Iq{});
    REQUIRE(bus.read().taps == std::array<uint8_t, 3>{0, 200, 0});
    REQUIRE(tap.read() == 200);

    // A field written back to its value is no change of the struct.
    tap.write(200);
    ctx->run(1);
    REQUIRE(changes == 3);

    // The bits of a field.
    BitSel low = tap[{3, 0}];
    low.write(0xf);
    ctx->run(1);
    REQUIRE(bus.read().taps[1] == 207);
    REQUIRE(low.read() == 0xf);
}

TEST_CASE("the fields of a struct signal by name", "[value][field]")
{
    auto ctx = Context::create();
    Signal<Bus> bus{"bus"};
    SignalBase &any = bus;

    SignalBase &tag = any.field("tag");
    REQUIRE(tag.name() == "bus.tag");
    REQUIRE(dynamic_cast<Signal<uint16_t> *>(&tag) != nullptr);
    // Asked for again, it is the same signal.
    REQUIRE(&any.field("tag") == &tag);

    // A nested struct is a signal with fields, and a struct and its first field are different signals.
    SignalBase &iq = any.field("iq");
    SignalBase &re = iq.field("re");
    REQUIRE(dynamic_cast<Signal<Iq> *>(&iq) != nullptr);
    REQUIRE(dynamic_cast<Signal<int16_t> *>(&re) != nullptr);
    REQUIRE(re.name() == "bus.iq.re");
    REQUIRE(&iq.field("re") == &re);
    REQUIRE(&any.field("iq") == &iq);
    REQUIRE(&iq != &re);

    // Other types: a bool, a double, an enumeration.
    REQUIRE(dynamic_cast<Signal<bool> *>(&any.field("valid")) != nullptr);
    REQUIRE(dynamic_cast<Signal<double> *>(&any.field("gain")) != nullptr);
    REQUIRE(dynamic_cast<Signal<Mode> *>(&any.field("mode")) != nullptr);

    // The elements of an array are signals.
    auto taps = any.field_array("taps");
    REQUIRE(taps.shape() == Shape{3});
    REQUIRE(taps.flat(2).name() == "bus.taps[2]");
    REQUIRE(&any.field_array("taps").flat(2) == &taps.flat(2));
    auto grid = any.field_array("grid");
    REQUIRE(grid.shape() == Shape{2, 2});
    REQUIRE(grid[{1, 0}].name() == "bus.grid[1][0]");
    SignalBase &grid_im = grid[{1, 0}].field("im");
    // A field that is not an array is an array of one.
    REQUIRE(any.field_array("tag").shape().empty());
    REQUIRE(&any.field_array("tag").flat(0) == &tag);

    REQUIRE_THROWS_WITH(any.field("nothing"), ContainsSubstring("has no field 'nothing'"));
    REQUIRE_THROWS_WITH(any.field("taps"), ContainsSubstring("is an array"));
    REQUIRE_THROWS_WITH(tag.field("x"), ContainsSubstring("has no fields"));
    Signal<Plain> plain{"plain"};
    REQUIRE_THROWS_WITH(plain.field("a"), ContainsSubstring("has no fields"));
    ctx->elaborate();

    Bus value;
    value.tag = 9;
    value.iq = {1, 2};
    value.taps = {4, 5, 6};
    value.grid[1][0] = {7, 8};
    value.mode = Mode::on;
    bus.write(value);
    ctx->run(1);
    REQUIRE(static_cast<Signal<uint16_t> &>(tag).read() == 9);
    REQUIRE(static_cast<Signal<Iq> &>(iq).read() == Iq{1, 2});
    REQUIRE(static_cast<Signal<int16_t> &>(re).read() == 1);
    REQUIRE(static_cast<Signal<uint8_t> &>(taps.flat(2)).read() == 6);
    REQUIRE(static_cast<Signal<int16_t> &>(grid_im).read() == 8);

    // Written as bytes, through to the struct.
    const int16_t im = -8;
    grid_im.write_data(&im);
    ctx->run(1);
    REQUIRE(bus.read().grid[1][0] == Iq{7, -8});

    // A field that exists is still there after elaboration; a new one cannot be made.
    REQUIRE(&any.field("tag") == &tag);
    REQUIRE_THROWS_WITH(any.field("data"), ContainsSubstring("before the design is elaborated"));
    REQUIRE_THROWS_WITH(bus.field(&Bus::data), ContainsSubstring("before the design is elaborated"));
}
