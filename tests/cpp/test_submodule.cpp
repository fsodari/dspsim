#include <dspsim/dspsim.h>

#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

class Sub : public Module
{
public:
    Input<uint8_t> clk;
    Input<uint8_t> in;
    Output<uint8_t> out;

    Sub(ModuleName name, Signal<uint8_t> &clk_, Signal<uint8_t> &in_, Signal<uint8_t> &out_)
        : Module(name),
          clk("clk", clk_),
          in("in", in_),
          out("out", out_)
    {
        always << clk.pos();
    }

    void eval() override
    {
        context()->logger->debug("Sub eval() called, time: {}", context()->time());
        out.write(in.read());
    }
};

class Parent : public Module
{
public:
    Input<uint8_t> clk;
    Input<uint8_t> in;
    Output<uint8_t> out;

    // Submodules must be initialized last.
    Sub sub;

    Parent(ModuleName name, Signal<uint8_t> &clk_, Signal<uint8_t> &in_, Signal<uint8_t> &out_)
        : Module(name),
          clk("clk", clk_),
          in("in", in_),
          out("out", out_),
          sub("sub", clk, in, out) // Submodule must be initialized last. Init with ports or signals.
    {
        always << clk.pos();
    }

    void eval() override
    {
        context()->logger->debug("Parent eval() called, time: {}", context()->time());
    }
};
TEST_CASE("test_submodule")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::warn);

    Clock clk{"clk", 10};
    Signal<uint8_t> in_signal{"in_signal"};
    Signal<uint8_t> out_signal{"out_signal"};

    Parent parent{"parent", clk, in_signal, out_signal};

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
