#include <dspsim/dspsim.h>
#include <dspsim/coro/coro.h>
#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

class SomeModule : public Module
{
public:
    ProcessBase *_coro_process;
    ProcessBase *_one_shot_process;
    ProcessBase *_clocked_task_process;

    Input<uint8_t> clk{"clk"};

    SomeModule(ModuleName name)
        : Module(name)
    {
        // context()->register_coro_task(some_task(), "some_task");
        _coro_process = DSPSIM_CORO(some_task);
        _one_shot_process = DSPSIM_CORO(one_shot);
        _clocked_task_process = DSPSIM_CORO(clocked_task);
    }

    Task some_task()
    {
        context()->logger->info("Starting some_task");
        while (true)
        {
            context()->logger->info("In some_task loop, t={}", context()->time());
            co_await wait{10, context(), _coro_process};
            context()->logger->info("After wait(10), t={}", context()->time());
            co_await wait{10, context(), _coro_process};
            context()->logger->info("After second wait(10), t={}", context()->time());
        }
        context()->logger->info("Exiting some_task loop, t={}", context()->time());
    }

    Task one_shot()
    {
        context()->logger->info("Starting one_shot");
        REQUIRE(context()->time() == 0);
        co_await context()->wait_(5);
        REQUIRE(context()->time() == 5);
        context()->logger->info("After one_shot wait(5), t={}", context()->time());

        co_return;
    }

    Task clocked_task()
    {
        context()->logger->info("Starting clocked_task");
        while (true)
        {
            co_await context()->wait_(clk.posedge_event());

            REQUIRE(clk.posedge());
            context()->logger->info("clk.pos() event at t={}, posedge?={}", context()->time(), clk.posedge());
        }
    }
};

TEST_CASE("test coro basic", "[coro]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::debug);

    Clock clk{"clk", 10};
    SomeModule some_module{"some_module"};
    some_module.clk.bind(clk);

    ctx->elaborate();

    ctx->run(21);

    // What happens when one_shot is dead? SEGFAULT.
    // Need to remove the process from everything that has a reference to it?
    some_module._one_shot_process->resume();
}
