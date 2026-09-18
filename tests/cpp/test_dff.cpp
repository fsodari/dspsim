#include <dspsim/dspsim.h>

#include <spdlog/spdlog.h>

#include <cassert>

using namespace dspsim;

template <typename T>
class Dff : public Module
{
public:
    Input<uint8_t> clk{"clk_port_dff"};
    Input<T> d{"d_port_dff"};
    Output<T> q{"q_port_dff"};

    Dff(ModuleName name) : Module(name)
    {
        sensitivity_list << clk.pos();
    }

    void eval() override
    {
        context()->logger->debug("Dff eval() on posedge, time: {}, d={}, clk={}", context()->time(), d.read(), clk.read());
        if (clk.read() == 1)
        {
            q.write(d.read());
        }
    }
};

int main()
{
    auto ctx = Context::obtain();
    ctx->logger->set_level(spdlog::level::debug);

    Clock clk{"clk", 10};
    Signal<uint8_t> d_top{"d_top"};
    Signal<uint8_t> q_top{"q_top"};
    Signal<uint8_t> q_top2{"q_top2"};

    Dff<uint8_t> dff_inst{"dff_inst"};
    dff_inst.clk.bind(clk);
    dff_inst.d.bind(d_top);
    dff_inst.q.bind(q_top);

    Dff<uint8_t> dff2{"dff2"};
    dff2.clk.bind(clk);
    dff2.d.bind(q_top);
    dff2.q.bind(q_top2);

    ctx->elaborate();

    for (auto &subscriber : clk._subscribers)
    {
        ctx->logger->debug("clk Subscriber: {}", subscriber->name());
    }
    // return 0;

    ctx->run(20);
    for (int i = 1; i < 3; i++)
    {
        d_top.write(i);
        ctx->run(5);
        // d signal will have changed before the clock posedge, but q shouldn't
        assert(q_top.read() != d_top.read());

        ctx->logger->debug("time={}, d_top: {}, q_top: {}, q_top2={}", ctx->time(), d_top.read(), q_top.read(), q_top2.read());
        ctx->run(5);

        // q should have changed at the clock edge.
        assert(q_top.read() == d_top.read());
        ctx->logger->debug("time={}, d_top: {}, q_top: {}, q_top2={}", ctx->time(), d_top.read(), q_top.read(), q_top2.read());

        // The second dff should lag the first.
        assert(q_top2.read() == q_top.read() - 1);
    }

    return 0;
}