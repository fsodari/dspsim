/*
    Processes are scheduled when they are registered, so they can be registered at any time:
    during construction, between run() calls, or from a running process.
*/
#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace dspsim;

namespace
{
    Task<> count_posedges(Context *ctx, Clock *clk, int *count, std::vector<uint64_t> *started)
    {
        started->push_back(ctx->time());
        while (true)
        {
            co_await ctx->wait(clk->pos());
            ++*count;
        }
    }

    // Registers another coroutine process from inside a running process.
    Task<> spawn_after(Context *ctx, Clock *clk, uint64_t delay, int *count, std::vector<uint64_t> *started)
    {
        co_await ctx->wait(delay);
        ctx->register_coro_task(count_posedges(ctx, clk, count, started), "spawned");
    }
}

TEST_CASE("A process registered before the first run is evaluated at t=0", "[process][late]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    ctx->elaborate();

    int evals = 0;
    ctx->register_process_func([&]
                               { ++evals; }, "late");
    ctx->run(0);
    REQUIRE(evals == 1);
}

TEST_CASE("A process registered after a run is evaluated when the simulation continues", "[process][late]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    ctx->elaborate();
    ctx->run(42);

    int evals = 0;
    ctx->register_process_func([&]
                               { ++evals; }, "late");
    REQUIRE(evals == 0);
    ctx->run(0);
    REQUIRE(evals == 1);
    REQUIRE(ctx->time() == 42);

    // Without sensitivity it is not evaluated again.
    ctx->run(100);
    REQUIRE(evals == 1);
}

TEST_CASE("Static sensitivity of a process registered after a run", "[process][late]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    ctx->elaborate();
    ctx->run(42);

    int initialized = 0;
    int event_only = 0;
    ctx->register_process_func([&]
                               { ++initialized; }, "initialized")
        ->always(clk.pos());
    ctx->register_process_func([&]
                               { ++event_only; }, "event_only")
        ->always(clk.pos())
        ->initialize(false);

    // initialize(false) took the process off the evaluation stack.
    ctx->run(0);
    REQUIRE(initialized == 1);
    REQUIRE(event_only == 0);

    // Posedges at t=50, 60, 70.
    ctx->run(30);
    REQUIRE(initialized == 4);
    REQUIRE(event_only == 3);
}

TEST_CASE("initialize(true) schedules the process again", "[process][late]")
{
    auto ctx = Context::create();
    ctx->elaborate();

    int evals = 0;
    auto *process = ctx->register_process_func([&]
                                               { ++evals; }, "p");
    process->initialize(false);
    REQUIRE_FALSE(process->initialize());
    ctx->run(0);
    REQUIRE(evals == 0);

    process->initialize(true);
    REQUIRE(process->initialize());
    ctx->run(0);
    REQUIRE(evals == 1);
}

TEST_CASE("A coroutine registered after a run starts at the current time", "[coro][late]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    ctx->elaborate();
    ctx->run(42);

    int count = 0;
    std::vector<uint64_t> started;
    ctx->register_coro_task(count_posedges(ctx.get(), &clk, &count, &started), "late");
    REQUIRE(started.empty());

    // Posedges at t=50, 60, 70.
    ctx->run(30);
    REQUIRE(started == std::vector<uint64_t>{42});
    REQUIRE(count == 3);
}

TEST_CASE("A process registered by a running process starts in the same delta cycle", "[coro][late]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    ctx->elaborate();

    int count = 0;
    std::vector<uint64_t> started;
    ctx->register_coro_task(spawn_after(ctx.get(), &clk, 25, &count, &started), "spawner");

    // Spawned at t=25. Posedges at t=30, 40, 50.
    ctx->run(55);
    REQUIRE(started == std::vector<uint64_t>{25});
    REQUIRE(count == 3);
}

TEST_CASE("Many processes registered by a running process", "[process][late]")
{
    auto ctx = Context::create();
    ctx->elaborate();

    // More than the reserved capacity of the evaluation stack, so it reallocates while being evaluated.
    const int n = 5000;
    int evals = 0;
    ctx->register_process_func(
        [&]
        {
            for (int i = 0; i < n; ++i)
            {
                ctx->register_process_func([&]
                                           { ++evals; }, "child");
            }
        },
        "parent");
    ctx->run(0);
    REQUIRE(evals == n);
}

TEST_CASE("clear() drops scheduled processes", "[context][late]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    ctx->elaborate();

    int evals = 0;
    ctx->register_process_func([&]
                               { ++evals; }, "never");
    // The process is scheduled but the context is cleared before it runs.
    ctx->clear();
    ctx->run(10);
    REQUIRE(evals == 0);
}
