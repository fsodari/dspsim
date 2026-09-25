#include <dspsim/dspsim.h>
#include <dspsim/dff.h>
#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

TEST_CASE("Test Running Processes", "[process]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::trace);

    Clock clk{"clk", 10};
    Signal<uint8_t> d_top{"d_top"};
    Signal<uint8_t> q_top{"q_top"};

    Dff<uint8_t> dff_inst{"dff_inst"};
    dff_inst.clk.bind(clk);
    dff_inst.d.bind(d_top);
    dff_inst.q.bind(q_top);

    ctx->elaborate();

    ctx->run(15);
    for (int i = 1; i < 3; i++)
    {
        d_top.write(i);
        ctx->run(5);
        // d signal will have changed before the clock posedge, but q shouldn't
        REQUIRE(q_top.read() != d_top.read());

        ctx->logger->debug("time={}, d_top: {}, q_top: {}", ctx->time(), d_top.read(), q_top.read());
        ctx->run(5);

        // q should have changed at the clock edge.
        REQUIRE(q_top.read() == d_top.read());
        ctx->logger->debug("time={}, d_top: {}, q_top: {}", ctx->time(), d_top.read(), q_top.read());
    }
}
