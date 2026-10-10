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
            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        Task<> eval()
        {
            while (true)
            {
                context()->logger->debug("Sub eval() called, time: {}", context()->time());
                out.write(in.read());
                co_await wait();
            }
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

            DSPSIM_CORO(eval)
                ->always(clk.pos());
        }

        Task<> eval()
        {
            while (true)
            {
                context()->logger->debug("Parent eval() called, time: {}", context()->time());
                co_await wait();
            }
        }
    };
} // namespace

TEST_CASE("test_submodule", "[submodule]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::debug);

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
}
