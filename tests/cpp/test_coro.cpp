#include <dspsim/dspsim.h>
#include <dspsim/coro.h>
#include <spdlog/spdlog.h>
#include <print>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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
            co_await WaitTimeEvent{10, _coro_process};
            context()->logger->info("After wait(10), t={}", context()->time());
            co_await WaitTimeEvent{10, _coro_process};
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
            co_await wait(clk.pos());

            REQUIRE(clk.pos());
            context()->logger->info("clk.pos() event at t={}, posedge?={:s}", context()->time(), clk.pos().has_happened());
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

    Input<uint8_t> d{"d"};
    Input<uint8_t> e{"e"};
    Output<uint8_t> f{"f"};

    DSPSIM_CTOR(StaticCoros)
    {
        DSPSIM_CORO(add)
            ->always(a, b);
        DSPSIM_CORO(add2);
    }

    Task add()
    {
        while (true)
        {
            co_await wait();
            c = a + b;
        }
    }
    Task add2()
    {
        while (true)
        {
            std::println("Before wait(d.change(), e.change()) at t={}", context()->time());
            co_await wait({d.change(), e.change()});
            f = d + e;
        }
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
            co_await wait(a.change());
            co_await wait(b.change());
            c = a + b;
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
    Signal<uint8_t> d{"d"};
    Signal<uint8_t> e{"e"};
    Signal<uint8_t> f{"f"};
    StaticCoros static_coros{"static_coros"};

    static_coros.a.bind(a);
    static_coros.b.bind(b);
    static_coros.c.bind(c);
    static_coros.d.bind(d);
    static_coros.e.bind(e);
    static_coros.f.bind(f);

    a.write(1);
    b.write(2);
    d.write(3);
    e.write(4);

    ctx->elaborate();

    ctx->run(0);
    REQUIRE(c.read() == 3);
    // REQUIRE(f.read() == 7);

    a.write(3);
    b.write(4);
    d.write(5);
    e.write(6);
    ctx->run(0);
    REQUIRE(c.read() == 7);
    REQUIRE(f.read() == 11);
}
namespace
{
    // A coroutine that throws after its first wait.
    DSPSIM_MODULE(ThrowingCoro)
    {
        Input<uint8_t> s{"s"};

        DSPSIM_CTOR(ThrowingCoro)
        {
            DSPSIM_CORO(body);
        }

        Task body()
        {
            co_await wait(s.change());
            throw std::runtime_error("coro boom");
        }
    };
} // namespace

TEST_CASE("an exception in a coroutine propagates instead of terminating", "[coro]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    ThrowingCoro m{"m"};
    m.s.bind(s);
    ctx->elaborate();
    ctx->run(1);

    s.write(1);
    REQUIRE_THROWS_WITH(ctx->run(1), "coro boom");
}
