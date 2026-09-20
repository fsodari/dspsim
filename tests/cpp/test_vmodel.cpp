#include <dspsim/dspsim.h>
#include <dspsim/dff.h>
#include <dspsim/modules/axis_tx.h>
#include <dspsim/modules/axis_rx.h>
#include <spdlog/spdlog.h>
#include "Skid.h"
#include "Skid2.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp> // Required header

using namespace dspsim;

template <typename T, typename SkidType>
class Top : public Module
{
public:
    Input<uint8_t> clk{"clk"};
    Input<uint8_t> rst{"rst"};

    //
    AxisTx<T> axis_tx{"axis_tx"};
    SkidType skid{"skid"};
    AxisRx<T> axis_rx{"axis_rx"};

    Signal<T> s_axis_tdata{"s_axis_tdata"};
    Signal<uint8_t> s_axis_tvalid{"s_axis_tvalid"};
    Signal<uint8_t> s_axis_tready{"s_axis_tready"};
    Signal<T> m_axis_tdata{"m_axis_tdata"};
    Signal<uint8_t> m_axis_tvalid{"m_axis_tvalid"};
    Signal<uint8_t> m_axis_tready{"m_axis_tready"};

    Top(ModuleName name) : Module(name)
    {
        // AxisTx
        axis_tx.clk.bind(clk);
        axis_tx.rst.bind(rst);
        axis_tx.m_axis_tdata.bind(s_axis_tdata);
        axis_tx.m_axis_tvalid.bind(s_axis_tvalid);
        axis_tx.m_axis_tready.bind(s_axis_tready);

        // Skid
        skid.clk.bind(clk);
        skid.rst.bind(rst);
        skid.s_axis_tdata.bind(s_axis_tdata);
        skid.s_axis_tvalid.bind(s_axis_tvalid);
        skid.s_axis_tready.bind(s_axis_tready);
        skid.m_axis_tdata.bind(m_axis_tdata);
        skid.m_axis_tvalid.bind(m_axis_tvalid);
        skid.m_axis_tready.bind(m_axis_tready);

        // AxisRx
        axis_rx.clk.bind(clk);
        axis_rx.rst.bind(rst);
        axis_rx.s_axis_tdata.bind(m_axis_tdata);
        axis_rx.s_axis_tvalid.bind(m_axis_tvalid);
        axis_rx.s_axis_tready.bind(m_axis_tready);
    }

    void ready(uint8_t r)
    {
        axis_rx.ready(r);
    }
    uint8_t ready() const
    {
        return axis_rx.ready();
    }
};

TEST_CASE("test_vmodel", "[vmodel]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::trace);

    Clock clk{"clk", 10};
    Signal<uint8_t> rst{"rst"};

    Top<uint32_t, Skid> top1{"top1"};
    Top<uint32_t, Skid2> top2{"top2"};

    top1.clk.bind(clk);
    top1.rst.bind(rst);
    top2.clk.bind(clk);
    top2.rst.bind(rst);

    top1.skid.open_trace("traces/skid.vcd");
    top2.skid.open_trace("traces/skid2.fst");

    ctx->elaborate();
    ctx->run(100);

    // top1.send({1, 2, 3, 4, 5});
    auto tx_data = std::initializer_list<uint32_t>{99, 81, 73, 64, 1, 55, 42, 33};

    top1.axis_tx.push_range(tx_data);
    top2.axis_tx.push_range(tx_data);
    ctx->run(100);
    top1.ready(1);
    top2.ready(1);
    ctx->run(50);
    top1.ready(0);
    top2.ready(0);
    ctx->run(50);
    top1.ready(1);
    top2.ready(1);
    ctx->run(200);

    REQUIRE_THAT(top1.axis_rx.fifo, Catch::Matchers::RangeEquals(tx_data));
    REQUIRE_THAT(top2.axis_rx.fifo, Catch::Matchers::RangeEquals(tx_data));
}
