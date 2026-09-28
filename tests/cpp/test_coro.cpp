#include <dspsim/dspsim.h>
#include <dspsim/coro/coro.h>
#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

class SomeModule : public Module
{
    ProcessBase *_coro_process;

public:
    ProcessBase *_one_shot_process;
    SomeModule(ModuleName name)
        : Module(name)
    {
        // context()->register_coro_task(some_task(), "some_task");
        _coro_process = DSPSIM_CORO(some_task);
        _one_shot_process = DSPSIM_CORO(one_shot);
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
        co_await wait{5, context()};
        context()->logger->info("After one_shot wait(5), t={}", context()->time());

        co_return;
    }
};

TEST_CASE("test coro basic", "[coro]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::debug);

    Clock clk{"clk", 10};
    SomeModule some_module{"some_module"};

    ctx->elaborate();

    ctx->run(21);

    // What happens when one_shot is dead? SEGFAULT.
    // Need to remove the process from everything that has a reference to it?
    // some_module._one_shot_process->resume();
}
