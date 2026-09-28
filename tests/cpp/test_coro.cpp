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

    DSPSIM_CTOR(SomeModule)
    {
        // context()->register_coro_task(some_task(), "some_task");
        _coro_process = DSPSIM_CORO(some_task);
        _one_shot_process = DSPSIM_CORO(one_shot);
        _clocked_task_process = DSPSIM_CORO(clocked_task);
    }

    Task some_task()
    {
        context()->logger->info("Starting some_task, t={}", context()->time());
        while (true)
        {
            co_await WaitTimeEvent{10, context(), _coro_process};
            context()->logger->info("After wait(10), t={}", context()->time());
            co_await WaitTimeEvent{10, context(), _coro_process};
            context()->logger->info("After second wait(10), t={}", context()->time());
        }
        context()->logger->info("Exiting some_task loop, t={}", context()->time());
    }

    Task one_shot()
    {
        // Tasks will be run at t=0 by default unless initialize(false) is called.
        context()->logger->info("Starting one_shot");
        REQUIRE(context()->time() == 0);

        co_await context()->wait(5);

        REQUIRE(context()->time() == 5);
        context()->logger->info("After one_shot wait(5), t={}", context()->time());

        // Task ends and can no longer be resumed.
        // If the task has static sensitivity, an error will occur in the logs when those events occur.
        // One shot tasks should only rely on dynamic sensitivity.
        co_return;
    }

    Task clocked_task()
    {
        context()->logger->info("Starting clocked_task");
        while (true)
        {
            co_await wait(clk.posedge_event());

            REQUIRE(clk.posedge());
            context()->logger->info("clk.pos() event at t={}, posedge?={}", context()->time(), clk.posedge());
        }
    }
};

#define DSPSIM_ALWAYS_BEGIN \
    while (true)            \
    {                       \
        co_await wait();

#define DSPSIM_ALWAYS_END \
    }

class StaticCoros : public Module
{
public:
    Input<uint8_t> a{"a"};
    Input<uint8_t> b{"b"};
    Output<uint8_t> c{"c"};

    DSPSIM_CTOR(StaticCoros)
    {
        DSPSIM_CORO(add)
            ->always(a, b);
    }

    Task add()
    {
        // while (true)
        // {
        //     co_await wait();
        //     c.write(a.read() + b.read());
        // }
        DSPSIM_ALWAYS_BEGIN
        c.write(a.read() + b.read());
        DSPSIM_ALWAYS_END
    }
};

class SerialAwait : public Module
{
public:
    Input<uint8_t> a{"a"};
    Input<uint8_t> b{"b"};
    Output<uint8_t> c{"c"};

    DSPSIM_CTOR(SerialAwait)
    {
        DSPSIM_CORO(add);
    }

    Task add()
    {
        while (true)
        {
            /*
                await_ready is always false right now.
                Need to be able to check the event, then clear it so that it doesn't continually evaluate true.
                What if multiple processes are reading the ports?
                What about signals?
                Is this even a useful feature? To be able to AND events?
            */
            co_await wait(a.change_event());
            co_await wait(b.change_event());
            c.write(a.read() + b.read());
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

TEST_CASE("test coro static sensitivity", "[coro]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::debug);

    Signal<uint8_t> a{"a"};
    Signal<uint8_t> b{"b"};
    Signal<uint8_t> c{"c"};
    StaticCoros static_coros{"static_coros"};

    static_coros.a.bind(a);
    static_coros.b.bind(b);
    static_coros.c.bind(c);

    a.write(1);
    b.write(2);

    ctx->elaborate();

    ctx->run(0);
    REQUIRE(c.read() == 3);

    a.write(3);
    b.write(4);
    ctx->run(0);
    REQUIRE(c.read() == 7);
}