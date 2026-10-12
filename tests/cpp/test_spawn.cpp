/*
    Context::spawn(): a task that runs as a process of its own and is destroyed when it completes.
*/
#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>
#include "engine_inspector.h"

using namespace dspsim;
using dspsim::testing::EngineInspector;

namespace
{
    // Waits for a number of rising clock edges, then records the time and ends.
    Task<> job(Context *context, Clock *clk, int edges, std::vector<uint64_t> *finished)
    {
        for (int i = 0; i < edges; ++i)
        {
            co_await context->wait(clk->pos());
        }
        finished->push_back(context->time());
    }

    // Spawns a job on every rising clock edge.
    Task<> dispatcher(Context *context, Clock *clk, int edges, std::vector<uint64_t> *finished)
    {
        while (true)
        {
            co_await context->wait(clk->pos());
            context->spawn(job(context, clk, edges, finished), "job");
        }
    }

    Task<> record(std::vector<int> *order, int value)
    {
        order->push_back(value);
        co_return;
    }

    Task<> spawn_and_record(Context *context, std::vector<int> *order)
    {
        order->push_back(1);
        context->spawn(record(order, 2));
        order->push_back(3);
        co_return;
    }

    // The event arrives before the timeout, which leaves the timeout in the time queue when the task ends.
    Task<> wait_with_timeout(Context *context, SensitivityEvent *event, int *woken)
    {
        co_await context->wait(*event, 1000);
        ++*woken;
    }

    Task<int> fail()
    {
        throw std::runtime_error("the task failed");
        co_return 0;
    }
} // namespace

TEST_CASE("a spawned task is destroyed when it completes", "[spawn]")
{
    auto ctx = Context::create();
    Clock clk{"clk", 10};
    std::vector<uint64_t> finished;
    ctx->register_coro_task(dispatcher(ctx.get(), &clk, 3, &finished), "dispatcher");
    ctx->elaborate();
    // The clock and the dispatcher.
    const std::size_t processes = EngineInspector::process_count(*ctx);

    // A job is spawned on every rising edge, from t=0 on, and ends on the third edge after that.
    ctx->run(1005);
    REQUIRE(finished.size() == 98);
    REQUIRE(finished.front() == 30);
    REQUIRE(finished.back() == 1000);
    // The jobs spawned on the last three edges are still running. The other 98 are gone.
    REQUIRE(EngineInspector::process_count(*ctx) == processes + 3);
}

TEST_CASE("a task spawned by a process is evaluated in the same delta cycle", "[spawn]")
{
    auto ctx = Context::create();
    std::vector<int> order;
    ctx->spawn(spawn_and_record(ctx.get(), &order));
    ctx->elaborate();
    REQUIRE(EngineInspector::process_count(*ctx) == 1);

    ctx->run(0);
    // The spawned task runs once the task that spawned it has suspended or ended.
    REQUIRE(order == std::vector<int>{1, 3, 2});
    // Both tasks ended without waiting, and both are gone.
    REQUIRE(EngineInspector::process_count(*ctx) == 0);
}

TEST_CASE("a spawned task that ends with a time event pending leaves nothing behind", "[spawn]")
{
    auto ctx = Context::create();
    SensitivityEvent event{ctx.get()};
    int woken = 0;
    ctx->spawn(wait_with_timeout(ctx.get(), &event, &woken));
    ctx->elaborate();
    ctx->run(10);
    REQUIRE(EngineInspector::pending_time_events(*ctx) == 1);

    event.notify();
    ctx->run(10);
    REQUIRE(woken == 1);
    REQUIRE(EngineInspector::process_count(*ctx) == 0);
    // The timeout referred to the process, so it went with it.
    REQUIRE(EngineInspector::pending_time_events(*ctx) == 0);

    // Running past the time of the timeout is safe.
    ctx->run(2000);
    REQUIRE(woken == 1);
}

TEST_CASE("an exception of a spawned task propagates, and the task is destroyed", "[spawn]")
{
    auto ctx = Context::create();
    ctx->elaborate();
    ctx->run(0);

    ctx->spawn(fail());
    REQUIRE_THROWS_AS(ctx->run(0), std::runtime_error);

    // The simulation is still usable, and the next run cleans up.
    ctx->run(10);
    REQUIRE(ctx->time() == 10);
    REQUIRE(EngineInspector::process_count(*ctx) == 0);
}

TEST_CASE("a registered coroutine process is kept when it completes", "[spawn]")
{
    // register_coro_task() returns a pointer, which has to stay valid.
    auto ctx = Context::create();
    std::vector<int> order;
    ProcessBase *process = ctx->register_coro_task(record(&order, 1), "kept");
    ctx->elaborate();
    ctx->run(0);
    REQUIRE(process->done());
    REQUIRE(EngineInspector::process_count(*ctx) == 1);
}
