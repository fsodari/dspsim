#include <dspsim/dspsim.h>
#include "StructArrayModel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace dspsim;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("pointers to the parts of a verilated port", "[vport][struct]")
{
    using namespace dspsim::detail;

    // An array, nested for more dimensions: the elements in row-major order.
    VlUnpacked<VlUnpacked<SData, 3>, 2> array;
    auto elements = vl_element_pointers(array);
    REQUIRE(elements.size() == 6);
    REQUIRE(elements[0] == &array[0][0]);
    REQUIRE(elements[4] == &array[1][1]);

    // An array of packed structs: one vector each, of three words here.
    VlUnpacked<VlWide<3>, 2> packed;
    auto words = vl_word_pointers<uint32_t>(packed, 2);
    REQUIRE(words.size() == 2);
    REQUIRE(words[0] == &packed[0][0]);
    REQUIRE(words[1] == &packed[1][0]);
    REQUIRE_THROWS_WITH(vl_word_pointers<uint32_t>(packed, 4), ContainsSubstring("not an array of 4 packed structs"));

}

TEST_CASE("verilated arrays of structs", "[vmodel][struct]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    StructArrayModel dut{"dut"};

    // The elements are structs, named after their place in the array.
    REQUIRE(dut.a_in.shape() == Shape{2, 3});
    REQUIRE(dut.a_in[{1, 2}].name() == "a_in[1][2]");
    REQUIRE(dut.a_in[{1, 2}].data.name() == "a_in[1][2].data");
    REQUIRE(dut.a_in[{1, 2}].data.width() == 12);
    REQUIRE(dut.a_in[{1, 2}].taps.shape() == Shape{2});
    REQUIRE(dut.a_in[{1, 2}].wide.shape() == Shape{4});
    REQUIRE(dut.a_in[{1, 2}].wide[{3}].width() == 4);
    REQUIRE(dut.f_in.shape() == Shape{4});
    REQUIRE(dut.w_in[{1}].wide.shape() == Shape{3});
    REQUIRE(dut.w_in[{1}].wide[{2}].width() == 6);
    REQUIRE(dut.o_in.iqs.shape() == Shape{3});
    REQUIRE(dut.o_in.iqs[{2}].re.name() == "o_in.iqs[2].re");
    // One field of every element, as a struct of arrays has it.
    REQUIRE(dut.a_in.input_array<int16_t>("data").shape() == Shape{2, 3});
    REQUIRE(dut.a_in.input_array<uint8_t>("taps").shape() == Shape{2, 3, 2});
    REQUIRE(dut.a_in.input_array<uint32_t>("wide").shape() == Shape{2, 3, 4});
    REQUIRE(dut.a_in.nested("iq").input_array<int8_t>("re").shape() == Shape{2, 3});
    REQUIRE(dut.w_out.output_array<uint32_t>("wide").shape() == Shape{2, 3});

    SignalStructArray a_in = dut.a_in.signals("a_in");
    SignalStructArray a_out = dut.a_out.signals("a_out");
    SignalStructArray f_in = dut.f_in.signals("f_in");
    SignalStructArray f_out = dut.f_out.signals("f_out");
    SignalStructArray w_in = dut.w_in.signals("w_in");
    SignalStructArray w_out = dut.w_out.signals("w_out");
    SignalStruct o_in = dut.o_in.signals("o_in");
    SignalStruct o_out = dut.o_out.signals("o_out");
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

    SECTION("unpacked structs")
    {
        // Element by element.
        for (std::size_t i = 0; i < 2; ++i)
        {
            for (std::size_t j = 0; j < 3; ++j)
            {
                const auto n = static_cast<int>(3 * i + j);
                const SignalStruct &element = a_in[{i, j}];
                element.signal<int16_t>("data") = static_cast<int16_t>(100 * n - 200);
                element.array<uint8_t>("taps").write(std::vector<uint8_t>{static_cast<uint8_t>(n), static_cast<uint8_t>(n + 10)});
                element.nested("iq").signal<int8_t>("re") = static_cast<int8_t>(n + 1);
                element.nested("iq").signal<int8_t>("im") = static_cast<int8_t>(-n);
                element.array<uint32_t>("wide")[{0}] = static_cast<uint32_t>(n);
                element.nested("flags").signal<uint8_t>("id") = static_cast<uint8_t>(n);
                element.nested("flags").signal<uint8_t>("last") = static_cast<uint8_t>(n & 1);
            }
        }
        ctx->run(20);

        for (std::size_t i = 0; i < 2; ++i)
        {
            for (std::size_t j = 0; j < 3; ++j)
            {
                const auto n = static_cast<int>(3 * i + j);
                const SignalStruct &element = a_out[{i, j}];
                REQUIRE(element.signal<int16_t>("data").read() == 100 * n - 200 + n);
                REQUIRE(element.array<uint8_t>("taps").read() == std::vector<uint8_t>{static_cast<uint8_t>(n + 10), static_cast<uint8_t>(n)});
                REQUIRE(element.nested("iq").signal<int8_t>("re").read() == -(n + 1));
                REQUIRE(element.nested("iq").signal<int8_t>("im").read() == -n + static_cast<int>(i));
                // ~wide, in 100 bits.
                REQUIRE(element.array<uint32_t>("wide")[{0}].read() == ~static_cast<uint32_t>(n));
                REQUIRE(element.array<uint32_t>("wide")[{3}].read() == 0xf);
                REQUIRE(element.nested("flags").signal<uint8_t>("id").read() == ((n + static_cast<int>(j)) & 7));
                REQUIRE(element.nested("flags").signal<uint8_t>("last").read() == (~n & 1));
            }
        }
        // The same signals, field by field.
        REQUIRE(a_out.array<int16_t>("data").read() == std::vector<int16_t>{-200, -99, 2, 103, 204, 305});
        REQUIRE(a_out.nested("iq").array<int8_t>("re").read() == std::vector<int8_t>{-1, -2, -3, -4, -5, -6});
    }

    SECTION("packed structs")
    {
        // Field by field: one value per element.
        f_in.array<uint8_t>("id").write(std::vector<uint8_t>{7, 6, 5, 4});
        f_in.array<uint8_t>("last").write(std::vector<uint8_t>{1, 0, 0, 1});
        // 70 bits in words of 32, 32 and 6 bits.
        w_in.array<uint32_t>("wide").write(std::vector<uint32_t>{1, 2, 3, 0x11111111u, 0x22222222u, 0x33});
        w_in.array<int16_t>("data").write(std::vector<int16_t>{-1000, 1000});
        w_in.nested("flags").array<uint8_t>("id").write(std::vector<uint8_t>{3, 7});
        w_in[{1}].nested("flags").signal<uint8_t>("last") = 1;
        ctx->run(20);

        REQUIRE(f_out.array<uint8_t>("id").read() == std::vector<uint8_t>{7, 7, 7, 7});
        REQUIRE(f_out.array<uint8_t>("last").read() == std::vector<uint8_t>{1, 0, 0, 1});
        REQUIRE(f_out[{3}].signal<uint8_t>("last").read() == 1);
        REQUIRE(w_out.array<uint32_t>("wide").read() == std::vector<uint32_t>{~1u, ~2u, 0x3c, 0xeeeeeeeeu, 0xddddddddu, 0x0c});
        REQUIRE(w_out[{1}].array<uint32_t>("wide").read() == std::vector<uint32_t>{0xeeeeeeeeu, 0xddddddddu, 0x0c});
        REQUIRE(w_out.array<int16_t>("data").read() == std::vector<int16_t>{-1000, 999});
        REQUIRE(w_out.nested("flags").array<uint8_t>("id").read() == std::vector<uint8_t>{4, 1});
        REQUIRE(w_out.nested("flags").array<uint8_t>("last").read() == std::vector<uint8_t>{0, 1});
    }

    SECTION("arrays of structs in a struct, a union and a packed array")
    {
        SignalStructArray &iqs = o_in.struct_array("iqs");
        iqs.array<int8_t>("re").write(std::vector<int8_t>{1, 2, 3});
        iqs.array<int8_t>("im").write(std::vector<int8_t>{-4, -5, -6});
        o_in.struct_array("flags")[{0}].signal<uint8_t>("id") = 2;
        o_in.struct_array("flags")[{1}].signal<uint8_t>("id") = 5;
        o_in.struct_array("flags")[{0}].signal<uint8_t>("last") = 1;
        o_in.signal<uint8_t>("valid") = 1;
        // The union as {op[15:12], imm[11:0]}: imm is 0x123.
        u_in = 0xa123;
        // Three 4-bit structs.
        pk_in = 0x1b7;
        ctx->run(20);

        REQUIRE(o_out.struct_array("iqs").array<int8_t>("re").read() == std::vector<int8_t>{-4, -5, -6});
        REQUIRE(o_out.struct_array("iqs")[{2}].signal<int8_t>("im").read() == 5);
        REQUIRE(o_out.struct_array("flags").array<uint8_t>("id").read() == std::vector<uint8_t>{5, 2});
        REQUIRE(o_out.struct_array("flags").array<uint8_t>("last").read() == std::vector<uint8_t>{0, 1});
        REQUIRE(o_out.signal<uint8_t>("valid").read() == 1);
        // op is the low 4 bits of the first byte of the union, imm is negated in 12 bits.
        REQUIRE(u_out.read() == 0x3edd);
        REQUIRE(pk_out.read() == 0x7b1);
    }
}
