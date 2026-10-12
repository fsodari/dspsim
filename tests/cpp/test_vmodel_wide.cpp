#include <dspsim/dspsim.h>
#include "WideModel.h"

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

TEST_CASE("wide port words", "[vport][wide]")
{
    using namespace dspsim::detail;
    STATIC_REQUIRE(vl_words<65> == 3);
    STATIC_REQUIRE(vl_words<96> == 3);
    STATIC_REQUIRE(vl_words<100> == 4);
    STATIC_REQUIRE(vl_words<128> == 4);
    STATIC_REQUIRE(vl_top_word_bits<65> == 1);
    STATIC_REQUIRE(vl_top_word_bits<96> == 32);
    STATIC_REQUIRE(vl_top_word_bits<100> == 4);
}

TEST_CASE("verilated wide ports", "[vmodel][wide]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    WideModel dut{"dut"};

    // The words are the last dimension, after the unpacked dimensions.
    REQUIRE(dut.a.shape() == Shape{4});
    REQUIRE(dut.sum.shape() == Shape{4});
    REQUIRE(dut.arr_in.shape() == Shape{3, 4});
    REQUIRE(dut.arr_out.shape() == Shape{3, 4});
    // The most significant word has the bits that remain.
    REQUIRE(dut.a[{0}].width() == 32);
    REQUIRE(dut.a[{3}].width() == 4);
    REQUIRE(dut.sum[{3}].width() == 5);
    REQUIRE(dut.arr_in[{2, 3}].width() == 32);

    SignalArray<uint32_t> a{"a", {4}}, b{"b", {4}}, sum{"sum", {4}}, s{"s", {4}}, neg{"neg", {4}};
    SignalArray<uint32_t> arr_in{"arr_in", {3, 4}}, arr_out{"arr_out", {3, 4}};
    dut.clk.bind(clk);
    dut.a.bind(a);
    dut.b.bind(b);
    dut.sum.bind(sum);
    dut.s.bind(s);
    dut.neg.bind(neg);
    dut.arr_in.bind(arr_in);
    dut.arr_out.bind(arr_out);
    ctx->elaborate();

    // 0x9_00000003_00000002_ffffffff + 1 carries from word 0 into word 1.
    a.write(std::vector<uint32_t>{0xffffffffu, 2, 3, 9});
    b.write(std::vector<uint32_t>{1, 0, 0, 0});
    // -1 as a 100-bit number is 0xf_ffffffff_ffffffff_ffffffff.
    s.write(std::vector<uint32_t>{1, 0, 0, 0});
    for (std::size_t i = 0; i < 3; ++i)
    {
        for (std::size_t w = 0; w < 4; ++w)
        {
            arr_in[{i, w}] = static_cast<uint32_t>(0x1000 * i + w);
        }
    }
    ctx->run(20);

    REQUIRE(sum.read() == std::vector<uint32_t>{0, 3, 3, 9});
    REQUIRE(neg.read() == std::vector<uint32_t>{0xffffffffu, 0xffffffffu, 0xffffffffu, 0xf});
    for (std::size_t i = 0; i < 3; ++i)
    {
        for (std::size_t w = 0; w < 4; ++w)
        {
            REQUIRE(arr_out[{i, w}].read() == ~static_cast<uint32_t>(0x1000 * i + w));
        }
    }

    // Bits above the port width are not part of the port: the model must not see them. With all 128 bits
    // set, an untruncated a would carry into bit 100 of the sum.
    a.write(std::vector<uint32_t>{0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu});
    b.write(std::vector<uint32_t>{0, 0, 0, 0});
    ctx->run(10);
    REQUIRE(sum.read() == std::vector<uint32_t>{0xffffffffu, 0xffffffffu, 0xffffffffu, 0xf});
}
