#include <dspsim/dspsim.h>

#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

namespace
{
    DSPSIM_MODULE(Sub)
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> in{"in"};
        Output<uint8_t> out{"out"};

        DSPSIM_CTOR(Sub)
        {
            DSPSIM_METHOD(eval)
                ->always(clk.pos());
        }

        void eval()
        {
            context()->logger->debug("Sub eval() called, time: {}", context()->time());
            out.write(in.read());
        }
    };

    DSPSIM_MODULE(Parent)
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> in{"in"};
        Output<uint8_t> out{"out"};

        // Submodules must be initialized last.
        Sub sub{"sub"};

        DSPSIM_CTOR(Parent)
        {
            sub.clk.bind(clk);
            sub.in.bind(in);
            sub.out.bind(out);
            DSPSIM_METHOD(eval)
                ->always(clk.pos());
        }

        void eval()
        {
            context()->logger->debug("Parent eval() called, time: {}", context()->time());
        }
    };
} // namespace

TEST_CASE("test_submodule")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::warn);

    Clock clk{"clk", 10};
    Signal<uint8_t> in_signal{"in_signal"};
    Signal<uint8_t> out_signal{"out_signal"};

    Parent parent{"parent"};
    parent.clk.bind(clk);
    parent.in.bind(in_signal);
    parent.out.bind(out_signal);

    ctx->elaborate();

    ctx->run(20);
    in_signal.write(99);
    ctx->run(20);
    REQUIRE(out_signal.read() == 99);

    // Clock clk{"clk", 10};
    // Signal<uint8_t> d_top{"d_top"};
    // Signal<uint8_t> q_top{"q_top"};
    // Signal<uint8_t> q_top2{"q_top2"};

    // Dff<uint8_t> dff_inst{"dff_inst"};
    // dff_inst.clk.bind(clk);
    // dff_inst.d.bind(d_top);
    // dff_inst.q.bind(q_top);

    // Dff<uint8_t> dff2{"dff2"};
    // dff2.clk.bind(clk);
    // dff2.d.bind(q_top);
    // dff2.q.bind(q_top2);

    // ctx->elaborate();

    // for (auto &subscriber : clk._subscribers)
    // {
    //     ctx->logger->debug("clk Subscriber: {}", subscriber->name());
    // }
    // // return 0;

    // ctx->run(20);
    // for (int i = 1; i < 3; i++)
    // {
    //     d_top.write(i);
    //     ctx->run(5);
    //     // d signal will have changed before the clock posedge, but q shouldn't
    //     assert(q_top.read() != d_top.read());

    //     ctx->logger->debug("time={}, d_top: {}, q_top: {}, q_top2={}", ctx->time(), d_top.read(), q_top.read(), q_top2.read());
    //     ctx->run(5);

    //     // q should have changed at the clock edge.
    //     assert(q_top.read() == d_top.read());
    //     ctx->logger->debug("time={}, d_top: {}, q_top: {}, q_top2={}", ctx->time(), d_top.read(), q_top.read(), q_top2.read());

    //     // The second dff should lag the first.
    //     assert(q_top2.read() == q_top.read() - 1);
    // }
}
