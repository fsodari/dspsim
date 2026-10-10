#include <dspsim/dspsim.h>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

using namespace dspsim;

namespace
{
    // Records (packed, lo) every time either input changes.
    DSPSIM_MODULE(PackProbe)
    {
        Input<uint32_t> packed{"packed", 16};
        Input<uint8_t> lo{"lo"};
        std::vector<std::pair<uint32_t, uint8_t>> seen;

        DSPSIM_CTOR(PackProbe)
        {
            DSPSIM_METHOD(eval)->always(packed, lo);
        }

        void eval()
        {
            seen.emplace_back(packed.read(), lo.read());
        }
    };

    // Counts the events of a single-bit input.
    DSPSIM_MODULE(EdgeCounter)
    {
        Input<uint8_t> bit{"bit", 1};
        int posedges = 0;
        int negedges = 0;
        int changes = 0;

        DSPSIM_CTOR(EdgeCounter)
        {
            DSPSIM_METHOD(on_pos)->always(bit.pos())->initialize(false);
            DSPSIM_METHOD(on_neg)->always(bit.neg())->initialize(false);
            DSPSIM_METHOD(on_change)->always(bit.change())->initialize(false);
        }

        void on_pos() { ++posedges; }
        void on_neg() { ++negedges; }
        void on_change() { ++changes; }
    };

    DSPSIM_MODULE(SignedInput)
    {
        Input<int32_t> in{"in", 24};

        DSPSIM_CTOR(SignedInput)
        {
        }
    };
} // namespace

TEST_CASE("input bound to a pack", "[derived]")
{
    auto ctx = Context::create();
    Signal<uint8_t> a{"a"};
    Signal<uint8_t> b{"b"};
    PackProbe probe{"probe"};
    probe.packed.bind(pack(a, b));
    probe.lo.bind(b);

    // Initializing a source after binding initializes the port.
    a.init(0x12);
    b.init(0x34);
    ctx->elaborate();
    REQUIRE(probe.packed.read() == 0x1234);

    // Writes before the first run are seen by the initial evaluation.
    a.write(0x56);
    ctx->run(0);
    REQUIRE(probe.packed.read() == 0x5634);
    REQUIRE(probe.seen.back() == std::pair<uint32_t, uint8_t>{0x5634, 0x34});

    SECTION("both sources change in the same cycle")
    {
        probe.seen.clear();
        a.write(0xAB);
        b.write(0xCD);
        ctx->run(0);
        // The pack changes in the same delta cycle as its sources, so the process runs once.
        REQUIRE(probe.seen == std::vector<std::pair<uint32_t, uint8_t>>{{0xABCD, 0xCD}});
    }

    SECTION("one source changes")
    {
        probe.seen.clear();
        b.write(0x01);
        ctx->run(0);
        REQUIRE(probe.seen == std::vector<std::pair<uint32_t, uint8_t>>{{0x5601, 0x01}});

        probe.seen.clear();
        a.write(0x02);
        ctx->run(0);
        REQUIRE(probe.seen == std::vector<std::pair<uint32_t, uint8_t>>{{0x0201, 0x01}});
    }

    SECTION("no change when the sources are rewritten with the same value")
    {
        probe.seen.clear();
        a.write(0x56);
        ctx->run(0);
        REQUIRE(probe.seen.empty());
    }
}

TEST_CASE("edges of a single-bit slice", "[derived]")
{
    auto ctx = Context::create();
    Signal<uint8_t> a{"a"};
    EdgeCounter counter{"counter"};
    counter.bit.bind(a[3]);
    ctx->elaborate();
    ctx->run(0);

    // Other bits changing do not trigger the slice's events.
    a.write(0x07);
    ctx->run(0);
    REQUIRE(counter.changes == 0);

    a.write(0x08);
    ctx->run(0);
    REQUIRE(counter.posedges == 1);
    REQUIRE(counter.negedges == 0);
    REQUIRE(counter.changes == 1);

    a.write(0x0F);
    ctx->run(0);
    REQUIRE(counter.changes == 1);

    a.write(0xF0);
    ctx->run(0);
    REQUIRE(counter.posedges == 1);
    REQUIRE(counter.negedges == 1);
    REQUIRE(counter.changes == 2);
}

TEST_CASE("signed ports sign extend the selection", "[derived]")
{
    auto ctx = Context::create();
    Signal<int16_t> tdata{"tdata", 16};
    Signal<uint8_t> tid{"tid"};
    SignedInput sink{"sink"};
    sink.in.bind(pack(tdata, tid));
    tdata.init(-11);
    tid.init(3);
    ctx->elaborate();

    REQUIRE(sink.in.read() == sext(0xFFF503, 24));

    tdata.write(0x1234);
    ctx->run(0);
    REQUIRE(sink.in.read() == 0x123403);
}

TEST_CASE("selections of a derived signal refer to its sources", "[derived]")
{
    auto ctx = Context::create();
    Signal<uint8_t> a{"a"};
    Signal<uint8_t> b{"b"};
    auto derived = DerivedSignal<uint16_t>::create("derived", pack(a, b));

    REQUIRE(derived->width() == 16);
    REQUIRE(BitSel(*derived).parts() == pack(a, b).parts());
    REQUIRE((*derived)[{11, 4}].parts() == pack(a, b)[{11, 4}].parts());

    // A derived signal must be wide enough for its selection.
    REQUIRE_THROWS_AS(DerivedSignal<uint8_t>::create("narrow", pack(a, b)), std::invalid_argument);
}

TEST_CASE("binding a port to a selection", "[derived]")
{
    auto ctx = Context::create();
    ctx->set_log_level("off");
    Signal<uint8_t> a{"a"};
    Signal<uint8_t> b{"b"};
    PackProbe probe{"probe"};

    // The selection must match the port width.
    REQUIRE_THROWS_AS(probe.packed.bind(a), std::invalid_argument);
    REQUIRE_THROWS_AS(probe.packed.bind(pack(a, b, a[0])), std::invalid_argument);

    probe.packed.bind(pack(a, b));
    probe.lo.bind(b);
    ctx->elaborate();

    // Binding after elaboration is not allowed.
    REQUIRE_THROWS_AS(probe.packed.bind(pack(b, a)), std::logic_error);
}

namespace
{
    // Drives a packed output and a plain output with the same value whenever go changes.
    DSPSIM_MODULE(PackDriver)
    {
        Input<uint8_t> go{"go"};
        Output<uint16_t> packed{"packed", 16};
        Output<uint8_t> direct{"direct"};

        DSPSIM_CTOR(PackDriver)
        {
            DSPSIM_METHOD(eval)->always(go)->initialize(false);
        }

        void eval()
        {
            packed.write(static_cast<uint16_t>(go.read() * 0x0101));
            direct.write(go.read());
        }
    };

    // Records (hi, lo, direct) every time any input changes.
    DSPSIM_MODULE(TripleProbe)
    {
        Input<uint8_t> hi{"hi"};
        Input<uint8_t> lo{"lo"};
        Input<uint8_t> direct{"direct"};
        std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> seen;

        DSPSIM_CTOR(TripleProbe)
        {
            DSPSIM_METHOD(eval)->always(hi, lo, direct)->initialize(false);
        }

        void eval()
        {
            seen.emplace_back(hi.read(), lo.read(), direct.read());
        }
    };

    // Writes a byte to an output whenever go changes.
    DSPSIM_MODULE(ByteDriver)
    {
        Input<uint8_t> go{"go"};
        Output<uint8_t> out{"out"};
        uint8_t value;

        ByteDriver(dspsim::ModuleName name, uint8_t value) : Module(name), value(value)
        {
            DSPSIM_METHOD(eval)->always(go)->initialize(false);
        }

        void eval()
        {
            out.write(go.read() ? value : 0);
        }
    };

    // Writes a 16-bit word to an output whenever go changes.
    DSPSIM_MODULE(WordDriver)
    {
        Input<uint8_t> go{"go"};
        Output<uint16_t> out{"out"};
        uint16_t value;

        WordDriver(dspsim::ModuleName name, uint16_t value) : Module(name), value(value)
        {
            DSPSIM_METHOD(eval)->always(go)->initialize(false);
        }

        void eval()
        {
            out.write(go.read() ? value : 0);
        }
    };

    DSPSIM_MODULE(SignedOutput)
    {
        Output<int32_t> out{"out", 24};

        DSPSIM_CTOR(SignedOutput)
        {
        }
    };
} // namespace

TEST_CASE("output bound to a pack", "[derived]")
{
    auto ctx = Context::create();
    Signal<uint8_t> go{"go"};
    Signal<uint8_t> a{"a"};
    Signal<uint8_t> b{"b"};
    Signal<uint8_t> c{"c"};
    PackDriver driver{"driver"};
    TripleProbe probe{"probe"};
    driver.go.bind(go);
    driver.packed.bind(pack(a, b));
    driver.direct.bind(c);
    probe.hi.bind(a);
    probe.lo.bind(b);
    probe.direct.bind(c);
    ctx->elaborate();
    ctx->run(0);

    go.write(0x12);
    ctx->run(0);
    REQUIRE(a.read() == 0x12);
    REQUIRE(b.read() == 0x12);
    REQUIRE(driver.packed.read() == 0x1212);
    // The pack's sources change in the same delta cycle as an ordinary output.
    REQUIRE(probe.seen == std::vector<std::tuple<uint8_t, uint8_t, uint8_t>>{{0x12, 0x12, 0x12}});

    // The output follows its sources when they are written directly.
    b.write(0x34);
    ctx->run(0);
    REQUIRE(driver.packed.read() == 0x1234);

    // Writing the output's current value back leaves the sources alone.
    probe.seen.clear();
    go.write(0x12);
    ctx->run(0);
    REQUIRE(a.read() == 0x12);
    REQUIRE(b.read() == 0x34);
}

TEST_CASE("outputs bound to slices of one signal", "[derived]")
{
    auto ctx = Context::create();
    Signal<uint8_t> go{"go"};
    Signal<uint16_t> bus{"bus"};
    ByteDriver hi{"hi", 0xAB};
    ByteDriver lo{"lo", 0xCD};
    hi.go.bind(go);
    lo.go.bind(go);
    hi.out.bind(bus[{15, 8}]);
    lo.out.bind(bus[{7, 0}]);
    ctx->elaborate();
    ctx->run(0);

    // Both outputs write in the same delta cycle and their bits combine.
    go.write(1);
    ctx->run(0);
    REQUIRE(bus.read() == 0xABCD);
    REQUIRE(hi.out.read() == 0xAB);
    REQUIRE(lo.out.read() == 0xCD);

    go.write(0);
    ctx->run(0);
    REQUIRE(bus.read() == 0x0000);

}

namespace
{
    // Writes a whole signal and a slice of it in the same eval, in either order.
    DSPSIM_MODULE(MixedDriver)
    {
        Input<uint8_t> go{"go"};
        Output<uint8_t> whole{"whole"};
        Output<uint8_t> bit0{"bit0", 1};
        uint8_t whole_value = 0;
        uint8_t bit_value = 0;
        bool slice_first = false;

        DSPSIM_CTOR(MixedDriver)
        {
            DSPSIM_METHOD(eval)->always(go.pos())->initialize(false);
        }

        void eval()
        {
            if (slice_first)
            {
                bit0.write(bit_value);
                whole.write(whole_value);
            }
            else
            {
                whole.write(whole_value);
                bit0.write(bit_value);
            }
        }
    };

    // Runs MixedDriver once from an initial bus value and returns the resulting bus value and edge counts.
    struct MixedResult
    {
        uint8_t bus;
        int posedges;
        int negedges;
    };

    MixedResult run_mixed(bool slice_first, uint8_t init, uint8_t whole_value, uint8_t bit_value)
    {
        auto ctx = Context::create();
        Signal<uint8_t> go{"go"};
        Signal<uint8_t> bus{"bus"};
        bus.init(init);
        MixedDriver driver{"driver"};
        EdgeCounter edges{"edges"};
        driver.slice_first = slice_first;
        driver.whole_value = whole_value;
        driver.bit_value = bit_value;
        driver.go.bind(go);
        driver.whole.bind(bus);
        driver.bit0.bind(bus[0]);
        edges.bit.bind(bus);
        ctx->elaborate();
        ctx->run(0);

        go.write(1);
        ctx->run(0);
        return {bus.read(), edges.posedges, edges.negedges};
    }
} // namespace

TEST_CASE("slice and whole-signal writes in the same cycle combine", "[derived]")
{
    // Slice writes go through to the source's pending value at write time, so the last write wins per bit.
    auto r = run_mixed(false, 0x00, 0x12, 1);
    REQUIRE(r.bus == 0x13);
    r = run_mixed(true, 0x00, 0x12, 1);
    REQUIRE(r.bus == 0x12);

    // Writing the slice to its current value still counts.
    r = run_mixed(false, 0x00, 0xFF, 0);
    REQUIRE(r.bus == 0xFE);

    // The source commits once, so there are no spurious edges in either order.
    r = run_mixed(false, 0x02, 0x04, 1);
    REQUIRE(r.bus == 0x05);
    REQUIRE(r.posedges == 0);
    REQUIRE(r.negedges == 0);
    r = run_mixed(true, 0x02, 0x04, 1);
    REQUIRE(r.bus == 0x04);
    REQUIRE(r.posedges == 0);
    REQUIRE(r.negedges == 0);
}

TEST_CASE("signed outputs bound to a pack", "[derived]")
{
    auto ctx = Context::create();
    Signal<int16_t> tdata{"tdata", 16};
    Signal<uint8_t> tid{"tid"};
    SignedOutput source{"source"};
    source.out.bind(pack(tdata, tid));
    ctx->elaborate();
    ctx->run(0);

    // -11 is 0xFFFFF5 in 24 bits.
    source.out.write(-11);
    ctx->run(0);
    REQUIRE(tdata.read() == -1);
    REQUIRE(tid.read() == 0xF5);
    REQUIRE(source.out.read() == -11);

    // Values are truncated to the selection's width.
    source.out.write(0x7123456);
    ctx->run(0);
    REQUIRE(tdata.read() == 0x1234);
    REQUIRE(tid.read() == 0x56);
    REQUIRE(source.out.read() == 0x123456);
}

namespace
{
    // Counts posedges of any signal.
    class PosedgeCounter : public Module
    {
    public:
        int posedges = 0;

        PosedgeCounter(ModuleName name, SignalBase &trigger) : Module(name)
        {
            DSPSIM_METHOD(eval)->always(trigger.pos())->initialize(false);
        }

        void eval() { ++posedges; }
    };
} // namespace

TEST_CASE("signals of selections", "[derived]")
{
    auto ctx = Context::create();
    Signal<uint16_t> a{"a"};
    Signal<uint64_t> wide{"wide"};

    // The untyped signal uses the smallest unsigned type that fits.
    SignalBase &bit = a[3].signal("bit3");
    REQUIRE(dynamic_cast<DerivedSignal<uint8_t> *>(&bit));
    REQUIRE(bit.width() == 1);
    REQUIRE(bit.name() == "bit3");
    REQUIRE(dynamic_cast<DerivedSignal<uint16_t> *>(&a[{15, 4}].signal()));
    REQUIRE(dynamic_cast<DerivedSignal<uint32_t> *>(&pack(a, a).signal()));
    REQUIRE(dynamic_cast<DerivedSignal<uint64_t> *>(&pack(wide[{47, 0}], a).signal()));

    auto &signed_bits = a[{11, 0}].signal<int16_t>();
    REQUIRE(signed_bits.width() == 12);
    // Derived signals have no value of their own.
    REQUIRE_THROWS_AS(signed_bits.init(1), std::logic_error);

    PosedgeCounter counter{"counter", bit};
    ctx->elaborate();
    REQUIRE_THROWS_AS(a[0].signal(), std::logic_error);
    ctx->run(0);

    a.write(0x0F00);
    ctx->run(0);
    REQUIRE(counter.posedges == 0);
    REQUIRE(signed_bits.read() == -256);

    a.write(0x0F08);
    ctx->run(0);
    REQUIRE(counter.posedges == 1);
    REQUIRE(static_cast<const Signal<uint8_t> &>(bit).read() == 1);
}
