/*
    ProcessBase::disable() and enable().
*/
#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include "engine_inspector.h"

using namespace dspsim;
using dspsim::testing::EngineInspector;

namespace
{
    // Counts the changes of s. The mode selects what else the method process asks for.
    DSPSIM_MODULE(Counter)
    {
        Input<uint8_t> s{"s"};
        Input<uint8_t> e{"e"};
        ProcessBase *process = nullptr;
        int evals = 0;
        // 0: static sensitivity only. 1: also next_trigger(e). 2: also next_trigger(time).
        int mode = 0;

        DSPSIM_CTOR(Counter)
        {
            process = DSPSIM_METHOD(eval)->always(s)->initialize(false);
        }

        void eval()
        {
            ++evals;
            if (mode == 1)
            {
                next_trigger(e.change());
            }
            else if (mode == 2)
            {
                next_trigger(25);
            }
        }
    };

    // The same with a coroutine that waits on its static sensitivity.
    DSPSIM_MODULE(CoroCounter)
    {
        Input<uint8_t> s{"s"};
        ProcessBase *process = nullptr;
        int wakeups = 0;

        DSPSIM_CTOR(CoroCounter)
        {
            process = DSPSIM_CORO(body)->always(s);
        }

        Task<> body()
        {
            while (true)
            {
                co_await wait();
                ++wakeups;
            }
        }
    };

    // A coroutine that waits on an event dynamically.
    DSPSIM_MODULE(DynamicWaiter)
    {
        Input<uint8_t> e{"e"};
        ProcessBase *process = nullptr;
        int wakeups = 0;

        DSPSIM_CTOR(DynamicWaiter)
        {
            process = DSPSIM_CORO(body);
        }

        Task<> body()
        {
            while (true)
            {
                co_await wait(e.change());
                ++wakeups;
            }
        }
    };
} // namespace

TEST_CASE("a disabled method process is not evaluated until it is enabled", "[process][disable]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"}, e{"e"};
    Counter m{"m"};
    m.s.bind(s);
    m.e.bind(e);
    ctx->elaborate();

    s.write(1);
    ctx->run(1);
    REQUIRE(m.evals == 1);

    m.process->disable();
    s.write(2);
    ctx->run(1);
    REQUIRE(m.evals == 1);

    // Enabling does not evaluate the process: the change of s while it was disabled is lost.
    m.process->enable();
    ctx->run(1);
    REQUIRE(m.evals == 1);

    s.write(3);
    ctx->run(1);
    REQUIRE(m.evals == 2);
}

TEST_CASE("disable() drops what a method process asked for with next_trigger()", "[process][disable]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"}, e{"e"};

    SECTION("an event")
    {
        Counter m{"m"};
        m.s.bind(s);
        m.e.bind(e);
        m.mode = 1;
        ctx->elaborate();
        s.write(1);
        ctx->run(1);
        REQUIRE(m.evals == 1);
        REQUIRE(EngineInspector::has_dynamic_subscribers(e.change()));

        m.process->disable();
        REQUIRE_FALSE(EngineInspector::has_dynamic_subscribers(e.change()));
        e.write(1);
        ctx->run(1);
        REQUIRE(m.evals == 1);
    }
    SECTION("a time")
    {
        Counter m{"m"};
        m.s.bind(s);
        m.e.bind(e);
        m.mode = 2;
        ctx->elaborate();
        s.write(1);
        ctx->run(1);
        REQUIRE(m.evals == 1);
        REQUIRE(EngineInspector::pending_time_events(*ctx) == 1);

        m.process->disable();
        REQUIRE(EngineInspector::pending_time_events(*ctx) == 0);
        ctx->run(100);
        REQUIRE(m.evals == 1);
    }
}

TEST_CASE("disable() takes a scheduled process off the evaluation stack", "[process][disable]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"}, e{"e"};
    Counter m{"m"};
    m.s.bind(s);
    m.e.bind(e);
    // Scheduled for the first evaluation.
    m.process->initialize(true);
    m.process->disable();
    ctx->elaborate();
    ctx->run(1);
    REQUIRE(m.evals == 0);

    // enable() and initialize(true) together start it again.
    m.process->enable()->initialize(true);
    ctx->run(1);
    REQUIRE(m.evals == 1);
}

TEST_CASE("a coroutine that waits on its static sensitivity can be disabled", "[coro][disable]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    CoroCounter m{"m"};
    m.s.bind(s);
    ctx->elaborate();
    ctx->run(1);

    s.write(1);
    ctx->run(1);
    REQUIRE(m.wakeups == 1);

    m.process->disable();
    // Disabling twice changes nothing.
    m.process->disable();
    s.write(2);
    ctx->run(1);
    REQUIRE(m.wakeups == 1);

    m.process->enable();
    s.write(3);
    ctx->run(1);
    REQUIRE(m.wakeups == 2);
}

TEST_CASE("a coroutine that waits for an event cannot be disabled", "[coro][disable]")
{
    auto ctx = Context::create();
    Signal<uint8_t> e{"e"};
    DynamicWaiter m{"m"};
    m.e.bind(e);
    ctx->elaborate();
    ctx->run(1);

    // It would lose the wait it is suspended in.
    REQUIRE_THROWS_AS(m.process->disable(), std::logic_error);

    // Nothing changed: the wait is still there.
    e.write(1);
    ctx->run(1);
    REQUIRE(m.wakeups == 1);
}
