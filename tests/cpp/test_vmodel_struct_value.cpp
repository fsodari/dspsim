#include "StructValueModel.h"
#include <dspsim/dspsim.h>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

using namespace dspsim;
using namespace value_model;

namespace
{
    // 100 bits as four words, least significant first.
    std::array<uint32_t, 4> inverted(const std::array<uint32_t, 4> &words, uint32_t top_mask)
    {
        return {~words[0], ~words[1], ~words[2], ~words[3] & top_mask};
    }
}

TEST_CASE("verilated struct ports as values", "[vmodel][struct][value]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    StructValueModel dut{"dut"};

    // One port and one signal per struct.
    Signal<Bus> u_in{"u_in"}, u_out{"u_out"};
    Signal<PackedBus> p_in{"p_in"}, p_out{"p_out"};
    Signal<PackedWide> w_in{"w_in"}, w_out{"w_out"};
    dut.clk.bind(clk);
    dut.u_in.bind(u_in);
    dut.u_out.bind(u_out);
    dut.p_in.bind(p_in);
    dut.p_out.bind(p_out);
    dut.w_in.bind(w_in);
    dut.w_out.bind(w_out);
    REQUIRE(dut.u_in.hier_name() == "root.dut.u_in");
    // The structs are described, for Python.
    REQUIRE(dut.u_in.value_type()->layout->find("iq")->type.layout->find("im") != nullptr);
    REQUIRE(dut.w_in.value_type()->layout->find("wide")->type.shape == Shape{4});
    ctx->elaborate();

    // Unpacked: a variable of the model per field.
    Bus u;
    u.data = -1234;
    u.valid = 1;
    u.taps = {1, 2, 3, 63};
    u.iq = {-100, 77};
    u.wide = {0x12345678u, 0x9abcdef0u, 0x0f0f0f0fu, 0x5u};
    u.flags = {5, 1};
    u_in.write(u);
    // Packed: 17 bits of one integer.
    p_in.write({-2047, 1, {6, 0}});
    // Packed and wide: 147 bits in 32-bit words, `mid` across two of them.
    PackedWide w;
    w.wide = {0xdeadbeefu, 0x01234567u, 0x89abcdefu, 0xau};
    w.mid = -(int64_t{1} << 38) + 12345;
    w.flags = {7, 1};
    w.lo = 7;
    w_in.write(w);
    ctx->run(10);

    const Bus &uo = u_out.read();
    REQUIRE(uo.data == 1234);
    REQUIRE(uo.valid == 1);
    REQUIRE(uo.taps == std::array<uint8_t, 4>{63, 3, 2, 1});
    REQUIRE(uo.iq == Iq{77, -100});
    REQUIRE(uo.wide == inverted(u.wide, 0xf));
    REQUIRE(uo.flags == Flags{6, 0});

    REQUIRE(p_out.read() == PackedBus{2047, 1, {0, 1}});

    const PackedWide &wo = w_out.read();
    REQUIRE(wo.wide == inverted(w.wide, 0xf));
    REQUIRE(wo.mid == -w.mid);
    REQUIRE(wo.flags == Flags{2, 1});
    REQUIRE(wo.lo == 0);

    // The bits above the width of a field do not reach the model: data has 12 bits, a tap 6.
    u = {};
    u.data = 0x7801;
    u.taps = {0xff, 0, 0, 0};
    u.flags = {0xff, 0xfe};
    u_in.write(u);
    ctx->run(10);
    REQUIRE(u_out.read().data == 0x7ff);
    REQUIRE(u_out.read().taps[3] == 63);
    REQUIRE(u_out.read().flags == Flags{0, 1});
}

TEST_CASE("arrays of verilated struct ports as values", "[vmodel][struct][value]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    StructArrayValueModel dut{"dut"};

    REQUIRE(dut.a_in.shape() == Shape{2, 3});
    REQUIRE(dut.a_in[{1, 2}].name() == "a_in[1][2]");
    REQUIRE(dut.w_in.shape() == Shape{2});

    SignalArray<Elem> a_in{"a_in", {2, 3}}, a_out{"a_out", {2, 3}};
    SignalArray<Flags> f_in{"f_in", {4}}, f_out{"f_out", {4}};
    SignalArray<PackedElem> w_in{"w_in", {2}}, w_out{"w_out", {2}};
    Signal<Outer> o_in{"o_in"}, o_out{"o_out"};
    Signal<uint16_t> u_in{"u_in"}, u_out{"u_out"}, pk_in{"pk_in", 12}, pk_out{"pk_out", 12};
    dut.clk.bind(clk);
    dut.a_in.bind(a_in);
    dut.a_out.bind(a_out);
    dut.f_in.bind(f_in);
    dut.f_out.bind(f_out);
    dut.w_in.bind(w_in);
    dut.w_out.bind(w_out);
    dut.o_in.bind(o_in);
    dut.o_out.bind(o_out);
    dut.u_in.bind(u_in);
    dut.u_out.bind(u_out);
    dut.pk_in.bind(pk_in);
    dut.pk_out.bind(pk_out);
    ctx->elaborate();

    for (std::size_t i = 0; i < 2; ++i)
    {
        for (std::size_t j = 0; j < 3; ++j)
        {
            Elem e;
            e.data = static_cast<int16_t>(100 * i + 10 * j - 50);
            e.taps = {static_cast<uint8_t>(i + 1), static_cast<uint8_t>(j + 10)};
            e.iq = {static_cast<int8_t>(i + j), static_cast<int8_t>(-5 - j)};
            e.wide = {static_cast<uint32_t>(i), static_cast<uint32_t>(j), 0xffff0000u, 0x3u};
            e.flags = {static_cast<uint8_t>(i + 2 * j), static_cast<uint8_t>(j % 2)};
            a_in[{i, j}].write(e);
        }
    }
    for (std::size_t i = 0; i < 4; ++i)
    {
        f_in[{i}].write({static_cast<uint8_t>(i + 3), static_cast<uint8_t>(i % 2)});
    }
    // 86 bits: wide[69:0], data, flags.
    w_in[{0}].write({{0x11111111u, 0x22222222u, 0x15u}, -7, {1, 1}});
    w_in[{1}].write({{0xffffffffu, 0u, 0x3fu}, 100, {7, 0}});
    Outer o;
    o.iqs = {Iq{1, 2}, Iq{3, 4}, Iq{5, 6}};
    o.flags = {Flags{1, 0}, Flags{6, 1}};
    o.valid = 1;
    o_in.write(o);
    ctx->run(10);

    for (std::size_t i = 0; i < 2; ++i)
    {
        for (std::size_t j = 0; j < 3; ++j)
        {
            const Elem &in = a_in[{i, j}].read();
            const Elem &out = a_out[{i, j}].read();
            REQUIRE(out.data == in.data + static_cast<int>(3 * i + j));
            REQUIRE(out.taps == std::array<uint8_t, 2>{in.taps[1], in.taps[0]});
            REQUIRE(out.iq == Iq{static_cast<int8_t>(-in.iq.re), static_cast<int8_t>(in.iq.im + i)});
            REQUIRE(out.wide == inverted(in.wide, 0xf));
            REQUIRE(out.flags == Flags{static_cast<uint8_t>((in.flags.id + j) % 8), static_cast<uint8_t>(1 - in.flags.last)});
        }
    }
    for (std::size_t i = 0; i < 4; ++i)
    {
        REQUIRE(f_out[{i}].read() == Flags{static_cast<uint8_t>((2 * i + 3) % 8), static_cast<uint8_t>(i % 2)});
    }
    REQUIRE(w_out[{0}].read() == PackedElem{{~0x11111111u, ~0x22222222u, 0x2au}, -7, {2, 1}});
    REQUIRE(w_out[{1}].read() == PackedElem{{0u, 0xffffffffu, 0u}, 99, {1, 0}});
    const Outer &oo = o_out.read();
    REQUIRE(oo.iqs == std::array<Iq, 3>{Iq{2, 1}, Iq{4, 4}, Iq{6, 7}});
    REQUIRE(oo.flags == std::array<Flags, 2>{Flags{6, 1}, Flags{1, 0}});
    REQUIRE(oo.valid == 1);
}
