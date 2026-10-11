#include <format>
#include <dspsim/dspsim.h>
#include <dspsim/coro.h>
#include <print>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "engine_inspector.h"

using namespace dspsim;
using dspsim::testing::EngineInspector;

class SomeModule : public Module
{
public:
    ProcessBase *coro_process;
    ProcessBase *one_shot_process;
    ProcessBase *clocked_task_process;

    Input<uint8_t> clk{"clk"};

    DSPSIM_CTOR(SomeModule)
    {
        // context()->register_coro_task(some_task(), "some_task");
        coro_process = DSPSIM_CORO(some_task);
        one_shot_process = DSPSIM_CORO(one_shot);
        clocked_task_process = DSPSIM_CORO(clocked_task);
    }

    Task<> some_task()
    {
        context()->log("info", std::format("Starting some_task, t={}", context()->time()));
        while (true)
        {
            co_await WaitTimeEvent{10, coro_process};
            context()->log("info", std::format("After wait(10), t={}", context()->time()));
            co_await WaitTimeEvent{10, coro_process};
            context()->log("info", std::format("After second wait(10), t={}", context()->time()));
        }
        context()->log("info", std::format("Exiting some_task loop, t={}", context()->time()));
    }

    Task<> one_shot()
    {
        // Tasks will be run at t=0 by default unless initialize(false) is called.
        context()->log("info", "Starting one_shot");
        REQUIRE(context()->time() == 0);

        co_await context()->wait(5);

        REQUIRE(context()->time() == 5);
        context()->log("info", std::format("After one_shot wait(5), t={}", context()->time()));

        // Task ends and can no longer be resumed. It is unsubscribed from its static events.
        co_return;
    }

    Task<> clocked_task()
    {
        context()->log("info", "Starting clocked_task");
        while (true)
        {
            co_await wait(clk.pos());

            REQUIRE(clk.pos());
            context()->log("info", std::format("clk.pos() event at t={}, posedge?={:s}", context()->time(), clk.pos().has_happened()));
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

    Task<> add()
    {
        while (true)
        {
            co_await wait();
            c = a + b;
        }
    }
    Task<> add2()
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

    Task<> add()
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
    ctx->set_log_level("debug");

    Clock clk{"clk", 10};
    SomeModule some_module{"some_module"};
    some_module.clk.bind(clk);

    ctx->elaborate();

    ctx->run(21);

    // one_shot has completed. Only the scheduler resumes processes, and it no longer has a reference to it.
    REQUIRE(some_module.one_shot_process->done());
}

TEST_CASE("test coro static sensitivity", "[coro]")
{
    auto ctx = Context::create();
    ctx->set_log_level("debug");

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
        ProcessBase *process = nullptr;

        DSPSIM_CTOR(ThrowingCoro)
        {
            process = DSPSIM_CORO(body);
        }

        Task<> body()
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

namespace
{
    // A coroutine with static sensitivity that completes after two events.
    DSPSIM_MODULE(OneShotStatic)
    {
        Input<uint8_t> s{"s"};
        int count = 0;
        ProcessBase *process = nullptr;

        DSPSIM_CTOR(OneShotStatic)
        {
            // Subscribed through the port, before it is bound.
            process = DSPSIM_CORO(body)->always(s)->initialize(false);
        }

        Task<> body()
        {
            ++count;
            co_await wait();
            ++count;
        }
    };
} // namespace

TEST_CASE("a completed coroutine unsubscribes from its static events", "[coro]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    Signal<uint8_t> direct{"direct"};
    OneShotStatic m{"m"};
    m.s.bind(s);
    // A second static event, on a signal.
    auto *process = m.process;
    process->always(direct.pos());
    ctx->elaborate();

    ctx->run(1);
    REQUIRE(m.count == 0);
    REQUIRE(EngineInspector::has_static_subscribers(s.change()));
    REQUIRE(EngineInspector::has_static_subscribers(direct.pos()));

    s.write(1);
    ctx->run(1);
    REQUIRE(m.count == 1);
    REQUIRE(EngineInspector::has_static_subscribers(s.change()));

    s.write(2);
    ctx->run(1);
    REQUIRE(m.count == 2);
    REQUIRE(process->done());
    REQUIRE_FALSE(EngineInspector::has_static_subscribers(s.change()));
    REQUIRE_FALSE(EngineInspector::has_static_subscribers(direct.pos()));
    const uint64_t wake_count = EngineInspector::wake_count(*process);

    // Further events do not schedule the completed process.
    s.write(3);
    direct.write(1);
    ctx->run(1);
    REQUIRE(m.count == 2);
    REQUIRE(EngineInspector::wake_count(*process) == wake_count);
}

TEST_CASE("a coroutine that throws unsubscribes from its static events", "[coro]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    ThrowingCoro m{"m"};
    m.s.bind(s);
    m.process->always(s.neg());
    ctx->elaborate();
    ctx->run(1);

    s.write(1);
    REQUIRE_THROWS_WITH(ctx->run(1), "coro boom");
    REQUIRE_FALSE(EngineInspector::has_static_subscribers(s.neg()));
}

namespace
{
    // A coroutine that completes while it still has a dynamic event and a time wait scheduled.
    DSPSIM_MODULE(LeftoverWaits)
    {
        Input<uint8_t> s{"s"};
        ProcessBase *process = nullptr;

        DSPSIM_CTOR(LeftoverWaits)
        {
            process = DSPSIM_CORO(body);
        }

        Task<> body()
        {
            // Constructing a leaf awaitable schedules its wait. These are scheduled, but never awaited.
            wait(s.change());
            wait(5);
            co_return;
        }
    };

    // Coroutines that call next_trigger(), which is only for method processes.
    DSPSIM_MODULE(NextTriggerCoro)
    {
        Input<uint8_t> s{"s"};
        bool use_time;

        NextTriggerCoro(ModuleName name, bool use_time) : Module(name), use_time(use_time)
        {
            DSPSIM_CORO(body);
        }

        Task<> body()
        {
            if (use_time)
            {
                next_trigger(5);
            }
            else
            {
                next_trigger(s.change());
            }
            co_return;
        }
    };
} // namespace

TEST_CASE("next_trigger() is not allowed in a coroutine process", "[coro]")
{
    const bool use_time = GENERATE(false, true);
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    NextTriggerCoro m{"m", use_time};
    m.s.bind(s);
    ctx->elaborate();

    REQUIRE_THROWS_AS(ctx->run(1), std::logic_error);
    // Nothing was scheduled, so the completed coroutine is not woken.
    REQUIRE_FALSE(EngineInspector::has_dynamic_subscribers(s.change()));
    s.write(1);
    ctx->run(10);
}

TEST_CASE("initialize(true) is not allowed on a completed coroutine", "[coro]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    LeftoverWaits m{"m"};
    m.s.bind(s);
    ctx->elaborate();

    // Allowed while the coroutine has not completed.
    m.process->initialize(false)->initialize(true);
    ctx->run(1);
    REQUIRE(m.process->done());

    REQUIRE_THROWS_AS(m.process->initialize(true), std::logic_error);
    m.process->initialize(false);
    ctx->run(1);
}

TEST_CASE("a completed coroutine is not woken by leftover waits", "[coro]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    LeftoverWaits m{"m"};
    m.s.bind(s);
    ctx->elaborate();

    ctx->run(1);
    REQUIRE(m.process->done());
    REQUIRE_FALSE(EngineInspector::has_dynamic_subscribers(s.change()));
    const uint64_t wake_count = EngineInspector::wake_count(*m.process);

    s.write(1);
    ctx->run(10);
    REQUIRE(EngineInspector::wake_count(*m.process) == wake_count);
}
