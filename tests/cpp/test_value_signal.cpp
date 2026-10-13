/*
    Signals and ports whose value is not a number: a struct, an array, an enumeration. The templates are
    instantiated here, outside of the core library.
*/
#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include <vector>

using namespace dspsim;

namespace
{
    struct Iq
    {
        int16_t re = 0;
        int16_t im = 0;

        bool operator==(const Iq &) const = default;
    };

    // A struct with a nested struct and an array.
    struct Sample
    {
        uint32_t data = 0;
        bool valid = false;
        Iq iq;
        std::array<uint8_t, 3> taps{};

        bool operator==(const Sample &) const = default;
    };

    enum class State : uint8_t
    {
        idle,
        busy,
        done
    };

    using Frame = std::array<uint16_t, 4>;

    Sample negate(const Sample &s)
    {
        return {s.data + 1, s.valid, {static_cast<int16_t>(-s.iq.re), static_cast<int16_t>(-s.iq.im)}, {s.taps[2], s.taps[1], s.taps[0]}};
    }

    // Combinational: out is a function of in. Counts its evaluations.
    class ValueStage : public Module
    {
    public:
        Input<Sample> in{"in"};
        Output<Sample> out{"out"};
        int evaluations = 0;

        ValueStage(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(eval)->always(in)->initialize(false);
        }

        void eval()
        {
            ++evaluations;
            out.write(negate(in.read()));
        }
    };

    // A stage inside a module with ports of the same type.
    class ValueWrapper : public Module
    {
    public:
        Input<Sample> in{"in"};
        Output<Sample> out{"out"};
        ValueStage stage{"stage"};

        ValueWrapper(ModuleName name) : Module(name)
        {
            stage.in.bind(in);
            stage.out.bind(out);
        }
    };

    // A coroutine that waits for the value to change.
    class ValueMonitor : public Module
    {
    public:
        Input<Sample> in{"in"};
        std::vector<uint32_t> seen;

        ValueMonitor(ModuleName name) : Module(name)
        {
            DSPSIM_CORO(run);
        }

        Task<> run()
        {
            while (true)
            {
                co_await wait(in.change());
                seen.push_back(in.read().data);
            }
        }
    };

    // Registers an array of values and a state on the clock.
    class ValueRegister : public Module
    {
    public:
        Input<uint8_t> clk{"clk"};
        InputArray<Iq> d{"d", {2, 2}};
        OutputArray<Iq> q{"q", {2, 2}};
        Output<State> state{"state"};
        Output<Frame> frame{"frame"};

        ValueRegister(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(eval)->always(clk.pos())->initialize(false);
        }

        void eval()
        {
            Frame next = frame.read();
            for (std::size_t i = 0; i < d.size(); ++i)
            {
                q.flat(i).write(d.flat(i).read());
                next[i] = static_cast<uint16_t>(next[i] + d.flat(i).read().re);
            }
            frame.write(next);
            state.write(state.read() == State::idle ? State::busy : State::done);
        }
    };

    // A struct port with a field whose value is a struct.
    template <template <typename> class Port>
    struct TaggedPorts : PortStruct
    {
        Port<uint8_t> tag;
        Port<Iq> iq;

        TaggedPorts(const std::string &name) : PortStruct(name), tag(name + ".tag"), iq(name + ".iq")
        {
            add("tag", tag);
            add("iq", iq);
        }
    };

    class TaggedStage : public Module
    {
    public:
        TaggedPorts<Input> in{"in"};
        TaggedPorts<Output> out{"out"};

        TaggedStage(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(eval)->always(in.tag, in.iq);
        }

        void eval()
        {
            out.tag.write(static_cast<uint8_t>(in.tag.read() + 1));
            out.iq.write({in.iq.read().im, in.iq.read().re});
        }
    };
}

TEST_CASE("a signal with a struct as its value", "[signal][value]")
{
    auto ctx = Context::create();
    Signal<Sample> x{"x"};
    Signal<Sample> y{"y", default_bitwidth<Sample>::value, {7, true, {1, -1}, {1, 2, 3}}};
    ctx->elaborate();

    REQUIRE(x.read() == Sample{});
    REQUIRE(y.read().data == 7);
    REQUIRE(y.read().taps[2] == 3);
    REQUIRE_FALSE(x.is_integral());

    // A write is pending until the update phase.
    const Sample value{42, true, {3, -4}, {9, 8, 7}};
    x.write(value);
    REQUIRE(x.read() == Sample{});
    REQUIRE(x.read_d_() == value);
    ctx->run(0);
    REQUIRE(x.read() == value);

    // init() sets the value at once.
    x.init({1, false, {0, 0}, {0, 0, 0}});
    REQUIRE(x.read().data == 1);
    REQUIRE(x.read_d_().data == 1);
}

TEST_CASE("a struct signal has one change event, whichever field changed", "[signal][value]")
{
    auto ctx = Context::create();
    Signal<Sample> in{"in"};
    Signal<Sample> out{"out"};
    ValueStage stage{"stage"};
    stage.in.bind(in);
    stage.out.bind(out);
    ctx->elaborate();
    ctx->run(1);
    REQUIRE(stage.evaluations == 0);

    Sample value{5, true, {10, 20}, {1, 2, 3}};
    in.write(value);
    ctx->run(1);
    REQUIRE(stage.evaluations == 1);
    REQUIRE(out.read() == negate(value));

    // The same value again is not a change.
    in.write(value);
    ctx->run(1);
    REQUIRE(stage.evaluations == 1);

    // Each field is part of the comparison: a nested one, an element of the array, a bool.
    value.iq.im = 21;
    in.write(value);
    ctx->run(1);
    REQUIRE(stage.evaluations == 2);
    REQUIRE(out.read().iq.im == -21);

    value.taps[0] = 99;
    in.write(value);
    ctx->run(1);
    REQUIRE(stage.evaluations == 3);
    REQUIRE(out.read().taps[2] == 99);

    value.valid = false;
    in.write(value);
    ctx->run(1);
    REQUIRE(stage.evaluations == 4);

    // The last write of an evaluation phase is the value, and going back to the committed value is no change.
    in.write(Sample{});
    in.write(value);
    ctx->run(1);
    REQUIRE(stage.evaluations == 4);
}

TEST_CASE("the padding of a struct is not part of its value", "[signal][value]")
{
    // The compiler puts a byte between b and c, so that c is aligned.
    struct Padded
    {
        uint16_t a = 0;
        uint8_t b = 0;
        uint32_t c = 0;

        bool operator==(const Padded &) const = default;
    };
    static_assert(sizeof(Padded) == 8 && offsetof(Padded, c) == 4);
    static_assert(!std::has_unique_object_representations_v<Padded>);

    // The same fields with different bytes in the padding.
    auto padded = [](unsigned char fill)
    {
        alignas(Padded) unsigned char bytes[sizeof(Padded)];
        std::memset(bytes, fill, sizeof(bytes));
        Padded *value = new (bytes) Padded;
        std::memset(bytes, fill, sizeof(bytes));
        value->a = 0x1234;
        value->b = 0x56;
        value->c = 0x789abcde;
        return *std::launder(value);
    };

    auto ctx = Context::create();
    Signal<Padded> x{"x"};
    int changes = 0;
    ctx->register_process_func([&] { ++changes; }, "changes")->always(x.change())->initialize(false);
    ctx->elaborate();

    x.write(padded(0x00));
    ctx->run(1);
    REQUIRE(changes == 1);
    REQUIRE(x.read().c == 0x789abcde);

    // Only the fields are compared: another padding is not a change.
    x.write(padded(0xff));
    ctx->run(1);
    REQUIRE(changes == 1);

    Padded next = padded(0xff);
    next.b = 0x57;
    x.write(next);
    ctx->run(1);
    REQUIRE(changes == 2);
    REQUIRE(x.read() == next);
}

TEST_CASE("ports with a struct as their value bind through the ports of a parent", "[port][value]")
{
    auto ctx = Context::create();
    Signal<Sample> in{"in"};
    Signal<Sample> mid{"mid"};
    Signal<Sample> out{"out"};
    ValueWrapper a{"a"};
    ValueStage b{"b"};
    a.in.bind(in);
    a.out.bind(mid);
    b.in.bind(mid);
    b.out.bind(out);
    ctx->elaborate();

    const Sample value{100, true, {3, -4}, {1, 2, 3}};
    in.write(value);
    ctx->run(1);
    REQUIRE(mid.read() == negate(value));
    REQUIRE(out.read() == negate(negate(value)));
    REQUIRE(a.out.read() == mid.read());
    REQUIRE(a.stage.evaluations == 1);
    REQUIRE(b.evaluations == 1);
}

TEST_CASE("a coroutine waits for a struct signal to change", "[coro][value]")
{
    auto ctx = Context::create();
    auto in = Signal<Sample>::create("in");
    ValueMonitor monitor{"monitor"};
    monitor.in.bind(*in);
    ctx->elaborate();
    ctx->run(1);
    REQUIRE(monitor.seen.empty());

    for (uint32_t i = 1; i <= 3; ++i)
    {
        in->write({i, false, {0, 0}, {0, 0, 0}});
        ctx->run(1);
        // Unchanged: the coroutine keeps waiting.
        in->write({i, false, {0, 0}, {0, 0, 0}});
        ctx->run(1);
    }
    REQUIRE(monitor.seen == std::vector<uint32_t>{1, 2, 3});
}

TEST_CASE("a struct signal has no edges", "[signal][value]")
{
    auto ctx = Context::create();
    Signal<Sample> x{"x"};
    int edges = 0;
    int changes = 0;
    ctx->register_process_func([&] { ++edges; }, "edges")->always(x.pos(), x.neg())->initialize(false);
    ctx->register_process_func([&] { ++changes; }, "changes")->always(x.change())->initialize(false);
    ctx->elaborate();

    x.write({1, true, {1, 1}, {1, 1, 1}});
    ctx->run(1);
    x.write(Sample{});
    ctx->run(1);
    REQUIRE(changes == 2);
    REQUIRE(edges == 0);
}

TEST_CASE("arrays of signals and ports with a struct as their value", "[array][value]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    SignalArray<Iq> d{"d", {2, 2}};
    auto q = SignalArray<Iq>::create("q", {2, 2}, default_bitwidth<Iq>::value, {1, 1});
    Signal<State> state{"state"};
    Signal<Frame> frame{"frame"};
    ValueRegister reg{"reg"};
    reg.clk.bind(clk);
    reg.d.bind(d);
    reg.q.bind(*q);
    reg.state.bind(state);
    reg.frame.bind(frame);
    ctx->elaborate();

    REQUIRE(d[{1, 0}].name() == "d[1][0]");
    REQUIRE(q->flat(3).read() == Iq{1, 1});
    REQUIRE(state.read() == State::idle);
    REQUIRE(frame.read() == Frame{});

    // Bulk access in row-major order.
    const std::vector<Iq> values{{1, -1}, {2, -2}, {3, -3}, {4, -4}};
    d.write(values);
    ctx->run(10);
    REQUIRE(q->read() == values);
    REQUIRE(reg.q[{1, 1}].read() == Iq{4, -4});
    REQUIRE(state.read() == State::busy);
    REQUIRE(frame.read() == Frame{1, 2, 3, 4});

    // A slice of the array, written with one value.
    d.slice({Slice{0, 1}}).write(Iq{9, 9});
    ctx->run(10);
    REQUIRE(q->read() == std::vector<Iq>{{9, 9}, {9, 9}, {3, -3}, {4, -4}});
    REQUIRE(state.read() == State::done);
    REQUIRE(frame.read() == Frame{10, 11, 6, 8});
}

TEST_CASE("a port struct with a field that has a struct as its value", "[struct][value]")
{
    auto ctx = Context::create();
    TaggedStage a{"a"};
    TaggedStage b{"b"};
    auto in = a.in.signals("in");
    auto mid = a.out.signals("mid");
    auto out = b.out.signals("out");
    a.in.bind(in);
    a.out.bind(mid);
    b.in.bind(mid);
    b.out.bind(out);
    ctx->elaborate();

    REQUIRE(in.signal<Iq>("iq").name() == "in.iq");
    in.signal<uint8_t>("tag") = 5;
    in.signal<Iq>("iq") = Iq{3, -4};
    ctx->run(1);
    REQUIRE(mid.signal<Iq>("iq").read() == Iq{-4, 3});
    REQUIRE(out.signal<uint8_t>("tag").read() == 7);
    REQUIRE(out.signal<Iq>("iq").read() == Iq{3, -4});
    // The type of a field is part of what bind() checks.
    REQUIRE_THROWS(in.signal<Sample>("iq"));
}
