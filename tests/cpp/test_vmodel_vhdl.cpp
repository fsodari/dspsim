#include <dspsim/dspsim.h>
#include "Skid.h"
#include "SkidVhdl.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <random>

using namespace dspsim;

TEST_CASE("verilated VHDL entity", "[vmodel][vhdl]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    Signal<uint8_t> rst{"rst", 1, 1};
    Signal<int32_t> tdata{"tdata", 24};
    Signal<uint8_t> tvalid{"tvalid"}, tready{"tready"};
    Signal<uint8_t> vhdl_ready{"vhdl_ready"}, vhdl_valid{"vhdl_valid"}, sv_ready{"sv_ready"}, sv_valid{"sv_valid"};
    Signal<int32_t> vhdl_data{"vhdl_data", 24}, sv_data{"sv_data", 24};

    // The same skid buffer in VHDL and in SystemVerilog.
    SkidVhdl vhdl{"vhdl"};
    Skid sv{"sv"};
    vhdl.clk.bind(clk);
    vhdl.rst.bind(rst);
    vhdl.s_axis_tdata.bind(tdata);
    vhdl.s_axis_tvalid.bind(tvalid);
    vhdl.s_axis_tready.bind(vhdl_ready);
    vhdl.m_axis_tdata.bind(vhdl_data);
    vhdl.m_axis_tvalid.bind(vhdl_valid);
    vhdl.m_axis_tready.bind(tready);
    sv.clk.bind(clk);
    sv.rst.bind(rst);
    sv.s_axis_tdata.bind(tdata);
    sv.s_axis_tvalid.bind(tvalid);
    sv.s_axis_tready.bind(sv_ready);
    sv.m_axis_tdata.bind(sv_data);
    sv.m_axis_tvalid.bind(sv_valid);
    sv.m_axis_tready.bind(tready);
    REQUIRE(vhdl.s_axis_tdata.width() == 24);

    ctx->elaborate();
    ctx->run(30);
    rst = 0;

    std::mt19937 rng{1};
    int transfers = 0;
    for (int i = 0; i < 5000; ++i)
    {
        tdata = static_cast<int32_t>(rng() % 20001) - 10000;
        tvalid = rng() % 5 < 3;
        tready = rng() % 2;
        ctx->run(10);
        REQUIRE(vhdl_ready.read() == sv_ready.read());
        REQUIRE(vhdl_valid.read() == sv_valid.read());
        if (vhdl_valid.read())
        {
            // Negative values too: the port is signed.
            REQUIRE(vhdl_data.read() == sv_data.read());
            transfers += tready.read();
        }
    }
    REQUIRE(transfers > 1000);
}
