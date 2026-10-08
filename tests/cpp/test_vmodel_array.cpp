#include <dspsim/dspsim.h>
#include "NDArrayModel.h"

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

namespace
{
    class ArrayTop : public Module
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> rst{"rst"};
        InputArray<uint8_t> a{"a", {2, 3, 4}};
        OutputArray<uint8_t> b{"b", {2, 3, 4}};

        NDArrayModel dut{"dut"};

        SignalArray<uint8_t> a_sig{"a_sig", {2, 3, 4}};
        SignalArray<uint8_t> b_sig{"b_sig", {2, 3, 4}};

        ArrayTop(ModuleName name) : Module(name)
        {
            dut.clk.bind(clk);
            dut.rst.bind(rst);
            dut.a.bind(a_sig);
            dut.b.bind(b_sig);
            a.bind(a_sig);
            b.bind(b_sig);
        }
    };
}

TEST_CASE("verilated array ports", "[vmodel][array]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    Signal<uint8_t> rst{"rst"};
    ArrayTop top{"top"};
    top.clk.bind(clk);
    top.rst.bind(rst);

    REQUIRE(top.dut.a.shape() == Shape{2, 3, 4});
    REQUIRE(top.dut.b.size() == 24);

    ctx->elaborate();
    top.dut.open_trace("traces/test_vmodel_array");

    for (std::size_t i = 0; i < 2; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            for (std::size_t k = 0; k < 4; ++k)
                top.a_sig[{i, j, k}] = 100 * i + 10 * j + k;

    ctx->run(30);

    for (std::size_t i = 0; i < 2; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            for (std::size_t k = 0; k < 4; ++k)
                REQUIRE(top.b_sig[{i, j, k}].read() == 100 * i + 10 * j + k);

    rst = 1;
    ctx->run(20);
    REQUIRE(top.b_sig[{1, 2, 3}].read() == 0);
}
