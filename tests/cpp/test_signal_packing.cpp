#include <dspsim/dspsim.h>
#include <dspsim/modules/dff.h>
#include <dspsim/modules/axis_tx.h>
#include <dspsim/modules/axis_rx.h>
#include <spdlog/spdlog.h>
#include "Skid.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp> // Required header

using namespace dspsim;

TEST_CASE("pack and slice vmodel ports", "[vmodel][derived]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::warn);

    Clock clk{"clk", 10};
    Signal<uint8_t> rst{"rst"};
    Signal<int16_t> s_axis_tdata{"s_axis_tdata", 16};
    Signal<uint8_t> s_axis_tid{"s_axis_tid", 8};
    Signal<uint8_t> s_axis_tvalid{"s_axis_tvalid", 1};
    Signal<uint8_t> s_axis_tready{"s_axis_tready", 1};
    Signal<int32_t> m_axis_tdata_full{"m_axis_tdata", 24};
    Signal<uint8_t> m_axis_tvalid{"m_axis_tvalid", 1};
    Signal<uint8_t> m_axis_tready{"m_axis_tready", 1};

    Skid skid{"skid"};

    skid.clk.bind(clk);
    skid.rst.bind(rst);

    // ----- Signal packing ideas -----
    skid.s_axis_tdata.bind(pack(s_axis_tdata, s_axis_tid));
    // -----

    skid.s_axis_tvalid.bind(s_axis_tvalid);
    skid.s_axis_tready.bind(s_axis_tready);

    skid.m_axis_tdata.bind(m_axis_tdata_full);
    skid.m_axis_tvalid.bind(m_axis_tvalid);
    skid.m_axis_tready.bind(m_axis_tready);

    // ----- Signal slicing idea
    auto m_axis_tdata = m_axis_tdata_full[{23, 8}];
    auto m_axis_tid = m_axis_tdata_full[{7, 0}];
    // -----

    ctx->elaborate();

    s_axis_tdata.write(-11);
    s_axis_tid.write(3);
    s_axis_tvalid.write(1);
    ctx->run(10);
    s_axis_tvalid.write(0);
    ctx->run(20);

    REQUIRE(sext(m_axis_tdata.read(), 16) == -11);
    REQUIRE(m_axis_tid.read() == 3);
}

TEST_CASE("unpack vmodel outputs", "[vmodel][derived]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::warn);

    Clock clk{"clk", 10};
    Signal<uint8_t> rst{"rst"};
    Signal<int32_t> s_axis_tdata{"s_axis_tdata", 24};
    Signal<uint8_t> s_axis_tvalid{"s_axis_tvalid", 1};
    Signal<uint8_t> s_axis_tready{"s_axis_tready", 1};
    Signal<int16_t> m_axis_tdata{"m_axis_tdata", 16};
    Signal<uint8_t> m_axis_tid{"m_axis_tid", 8};
    Signal<uint8_t> m_axis_tvalid{"m_axis_tvalid", 1};
    Signal<uint8_t> m_axis_tready{"m_axis_tready", 1};

    Skid skid{"skid"};

    skid.clk.bind(clk);
    skid.rst.bind(rst);
    skid.s_axis_tdata.bind(s_axis_tdata);
    skid.s_axis_tvalid.bind(s_axis_tvalid);
    skid.s_axis_tready.bind(s_axis_tready);
    // The output drives both signals.
    skid.m_axis_tdata.bind(pack(m_axis_tdata, m_axis_tid));
    skid.m_axis_tvalid.bind(m_axis_tvalid);
    skid.m_axis_tready.bind(m_axis_tready);

    ctx->elaborate();

    // {-11, 3} as a 24-bit value.
    s_axis_tdata.write(static_cast<int32_t>(sext(0xFFF503, 24)));
    s_axis_tvalid.write(1);
    ctx->run(10);
    s_axis_tvalid.write(0);
    ctx->run(20);

    REQUIRE(m_axis_tdata.read() == -11);
    REQUIRE(m_axis_tid.read() == 3);
}
