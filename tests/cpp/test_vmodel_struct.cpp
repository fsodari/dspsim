#include <dspsim/dspsim.h>
#include "StructModel.h"

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

TEST_CASE("bits of a verilated packed port", "[vport][struct]")
{
    using namespace dspsim::detail;

    // One integer.
    uint32_t word = 0;
    vl_set_bits<12>(&word, 5, 0xabc);
    REQUIRE(word == 0xabcu << 5);
    vl_set_bits<1>(&word, 4, 1);
    vl_set_bits<4>(&word, 0, 0xf);
    REQUIRE(word == ((0xabcu << 5) | 0x1f));
    // Writing a field keeps the others, and takes only its own bits of the value.
    vl_set_bits<12>(&word, 5, 0xfffff123);
    REQUIRE(word == ((0x123u << 5) | 0x1f));
    REQUIRE(vl_get_bits<12>(&word, 5) == 0x123);
    REQUIRE(vl_get_bits<1>(&word, 4) == 1);
    REQUIRE(vl_get_bits<4>(&word, 0) == 0xf);

    uint8_t byte = 0xff;
    vl_set_bits<3>(&byte, 1, 0);
    REQUIRE(byte == 0xf1);
    uint64_t quad = 0;
    vl_set_bits<64>(&quad, 0, ~0ull);
    REQUIRE(quad == ~0ull);
    REQUIRE(vl_get_bits<64>(&quad, 0) == ~0ull);

    // 32-bit words: a field can lie across two or three of them.
    uint32_t words[5] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
    vl_set_bits<40>(words, 7, 0x12'3456'789aull);
    REQUIRE(vl_get_bits<40>(words, 7) == 0x12'3456'789aull);
    REQUIRE(words[0] == ((0x3456'789au << 7) | 0x7f));
    REQUIRE(words[1] == (0xffff8000u | (0x12'3456'789aull >> 25)));
    REQUIRE(words[2] == 0xffffffffu);
    vl_set_bits<64>(words, 47, 0x0123'4567'89ab'cdefull);
    REQUIRE(vl_get_bits<64>(words, 47) == 0x0123'4567'89ab'cdefull);
    REQUIRE(vl_get_bits<40>(words, 7) == 0x12'3456'789aull);
    REQUIRE(words[3] == (0xffff8000u | (0x0123'4567'89ab'cdefull >> 49)));
    REQUIRE(words[4] == 0xffffffffu);
}

TEST_CASE("verilated struct ports", "[vmodel][struct]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    StructModel dut{"dut"};

    // Each field is a port, named after its place in the struct.
    REQUIRE(dut.u_in.data.name() == "u_in.data");
    REQUIRE(dut.u_in.data.width() == 12);
    REQUIRE(dut.u_in.taps.shape() == Shape{4});
    REQUIRE(dut.u_in.wide.shape() == Shape{4});
    REQUIRE(dut.u_in.iq.re.hier_name() == "root.dut.u_in.iq.re");
    REQUIRE(dut.p_in.flags.id.width() == 3);
    REQUIRE(dut.w_in.wide.shape() == Shape{4});
    REQUIRE(dut.w_in.wide[{3}].width() == 4);
    REQUIRE(dut.w_out.mid.width() == 40);

    auto u_in = dut.u_in.signals("u_in");
    auto u_out = dut.u_out.signals("u_out");
    auto p_in = dut.p_in.signals("p_in");
    auto p_out = dut.p_out.signals("p_out");
    auto w_in = dut.w_in.signals("w_in");
    auto w_out = dut.w_out.signals("w_out");
    dut.clk.bind(clk);
    dut.u_in.bind(u_in);
    dut.u_out.bind(u_out);
    dut.p_in.bind(p_in);
    dut.p_out.bind(p_out);
    dut.w_in.bind(w_in);
    dut.w_out.bind(w_out);
    ctx->elaborate();

    SECTION("unpacked struct")
    {
        u_in.signal<int16_t>("data") = -1000;
        u_in.signal<uint8_t>("valid") = 1;
        u_in.array<uint8_t>("taps").write(std::vector<uint8_t>{1, 2, 3, 63});
        u_in.nested("iq").signal<int8_t>("re") = -5;
        u_in.nested("iq").signal<int8_t>("im") = 7;
        u_in.array<uint32_t>("wide").write(std::vector<uint32_t>{0x11111111u, 0x22222222u, 0x33333333u, 0x4});
        u_in.nested("flags").signal<uint8_t>("id") = 6;
        u_in.nested("flags").signal<uint8_t>("last") = 0;
        ctx->run(20);

        REQUIRE(u_out.signal<int16_t>("data").read() == 1000);
        REQUIRE(u_out.signal<uint8_t>("valid").read() == 1);
        REQUIRE(u_out.array<uint8_t>("taps").read() == std::vector<uint8_t>{63, 3, 2, 1});
        REQUIRE(u_out.nested("iq").signal<int8_t>("re").read() == 7);
        REQUIRE(u_out.nested("iq").signal<int8_t>("im").read() == -5);
        // ~wide, in 100 bits.
        REQUIRE(u_out.array<uint32_t>("wide").read() == std::vector<uint32_t>{0xeeeeeeeeu, 0xddddddddu, 0xccccccccu, 0xb});
        REQUIRE(u_out.nested("flags").signal<uint8_t>("id").read() == 7);
        REQUIRE(u_out.nested("flags").signal<uint8_t>("last").read() == 1);
    }

    SECTION("packed struct")
    {
        p_in.signal<int16_t>("data") = -2047;
        p_in.signal<uint8_t>("valid") = 1;
        p_in.nested("flags").signal<uint8_t>("id") = 7;
        p_in.nested("flags").signal<uint8_t>("last") = 1;
        ctx->run(20);

        REQUIRE(p_out.signal<int16_t>("data").read() == 2047);
        REQUIRE(p_out.signal<uint8_t>("valid").read() == 1);
        // 7 + 2 in 3 bits.
        REQUIRE(p_out.nested("flags").signal<uint8_t>("id").read() == 1);
        REQUIRE(p_out.nested("flags").signal<uint8_t>("last").read() == 0);

        // One field at a time: the others keep their values.
        p_in.signal<uint8_t>("valid") = 0;
        ctx->run(10);
        REQUIRE(p_out.signal<int16_t>("data").read() == 2047);
        REQUIRE(p_out.signal<uint8_t>("valid").read() == 0);
        REQUIRE(p_out.nested("flags").signal<uint8_t>("id").read() == 1);
    }

    SECTION("packed struct of more than 64 bits")
    {
        w_in.array<uint32_t>("wide").write(std::vector<uint32_t>{0x11111111u, 0x22222222u, 0x33333333u, 0x4});
        w_in.signal<int64_t>("mid") = -0x12'3456'789all;
        w_in.nested("flags").signal<uint8_t>("id") = 2;
        w_in.nested("flags").signal<uint8_t>("last") = 1;
        w_in.signal<uint8_t>("lo") = 7;
        ctx->run(20);

        REQUIRE(w_out.array<uint32_t>("wide").read() == std::vector<uint32_t>{0xeeeeeeeeu, 0xddddddddu, 0xccccccccu, 0xb});
        REQUIRE(w_out.signal<int64_t>("mid").read() == 0x12'3456'789all);
        REQUIRE(w_out.nested("flags").signal<uint8_t>("id").read() == 5);
        REQUIRE(w_out.nested("flags").signal<uint8_t>("last").read() == 1);
        REQUIRE(w_out.signal<uint8_t>("lo").read() == 0);

        // The most negative and the largest 40-bit numbers.
        w_in.signal<int64_t>("mid") = -(1ll << 39) + 1;
        ctx->run(10);
        REQUIRE(w_out.signal<int64_t>("mid").read() == (1ll << 39) - 1);
        REQUIRE(w_out.signal<uint8_t>("lo").read() == 0);
    }
}
