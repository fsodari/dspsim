/*
    Asynchronous operations: nested Task<T>, waits with timeouts, Context::run_until(), AxisRx::receive().
*/
#include <dspsim/dspsim.h>
#include <dspsim/modules/axis_rx.h>
#include <dspsim/modules/axis_tx.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stdexcept>
#include <vector>
#include "engine_inspector.h"

using namespace dspsim;
using dspsim::testing::EngineInspector;

namespace
{
    // An AxisTx wired straight into an AxisRx.
    template <typename T>
    DSPSIM_MODULE(AxisLoop)
    {
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> rst{"rst"};
        AxisTx<T> tx{"tx"};
        AxisRx<T> rx{"rx"};
        Signal<T> tdata{"tdata"};
        Signal<uint8_t> tvalid{"tvalid"};
        Signal<uint8_t> tready{"tready"};

        DSPSIM_CTOR(AxisLoop)
        {
            tx.clk.bind(clk);
            tx.rst.bind(rst);
            tx.m_axis_tdata.bind(tdata);
            tx.m_axis_tvalid.bind(tvalid);
            tx.m_axis_tready.bind(tready);
            rx.clk.bind(clk);
            rx.rst.bind(rst);
            rx.s_axis_tdata.bind(tdata);
            rx.s_axis_tvalid.bind(tvalid);
            rx.s_axis_tready.bind(tready);
        }
    };

    struct AxisBench
    {
        std::shared_ptr<Context> ctx = Context::create();
        Clock clk{"clk", 10};
        Signal<uint8_t> rst{"rst"};
        AxisLoop<int32_t> loop{"loop"};

        AxisBench()
        {
            loop.clk.bind(clk);
            loop.rst.bind(rst);
            ctx->elaborate();
        }
    };

    // Awaits nested tasks: one that returns a value and one that throws.
    DSPSIM_MODULE(NestedTasks)
    {
        int value = 0;
        std::string error;
        uint64_t value_time = 0;

        DSPSIM_CTOR(NestedTasks)
        {
            DSPSIM_CORO(body);
        }

        Task<int> compute()
        {
            co_await wait(5);
            int partial = co_await deeper();
            co_await wait(5);
            co_return partial * 2;
        }
        Task<int> deeper()
        {
            co_await wait(7);
            co_return 21;
        }
        Task<> fail()
        {
            co_await wait(1);
            throw std::runtime_error("nested boom");
        }

        Task<> body()
        {
            value = co_await compute();
            value_time = context()->time();
            try
            {
                co_await fail();
            }
            catch (const std::runtime_error &e)
            {
                error = e.what();
            }
        }
    };

    // Waits on e with a timeout, then on f. Records the results and the number of resumes.
    DSPSIM_MODULE(TimeoutWaiter)
    {
        Input<uint8_t> e{"e"};
        Input<uint8_t> f{"f"};
        uint64_t timeout;
        std::vector<WaitResult> results;
        int wakeups = 0;

        TimeoutWaiter(ModuleName name, uint64_t timeout) : Module(name), timeout(timeout)
        {
            DSPSIM_CORO(body);
        }

        Task<> body()
        {
            results.push_back(co_await wait(e.change(), timeout));
            ++wakeups;
            co_await wait(f.change());
            ++wakeups;
        }
    };

    // Statically sensitive to s, but first waits on time.
    DSPSIM_MODULE(StaticAndTime)
    {
        Input<uint8_t> s{"s"};
        int wakeups = 0;

        DSPSIM_CTOR(StaticAndTime)
        {
            DSPSIM_CORO(body)->always(s.change());
        }

        Task<> body()
        {
            co_await wait(50);
            ++wakeups;
            while (true)
            {
                co_await wait();
                ++wakeups;
            }
        }
    };
} // namespace

TEST_CASE("run_until drives a task to completion and returns its value", "[async][run_until]")
{
    AxisBench b;
    std::vector<int32_t> data{1, -2, 3, -4, 5};
    b.loop.rx.ready(1);
    b.loop.tx.push_range(data);
    const size_t n_processes = EngineInspector::process_count(*b.ctx);

    auto rx = b.ctx->run_until(b.loop.rx.receive(data.size(), 1000));
    REQUIRE_THAT(rx, Catch::Matchers::RangeEquals(data));
    REQUIRE(b.ctx->time() < 1000);
    REQUIRE(b.loop.rx.empty());

    // The one-shot process was removed, and the simulation keeps working.
    REQUIRE(EngineInspector::process_count(*b.ctx) == n_processes);
    b.loop.tx.push_range(std::vector<int32_t>{7, 8});
    REQUIRE_THAT(b.ctx->run_until(b.loop.rx.receive(2, 1000)), Catch::Matchers::RangeEquals(std::vector<int32_t>{7, 8}));
}

TEST_CASE("receive returns what arrived when its timeout elapses", "[async][axis]")
{
    AxisBench b;
    b.loop.rx.ready(1);
    b.loop.tx.push_range(std::vector<int32_t>{1, 2, 3});

    auto rx = b.ctx->run_until(b.loop.rx.receive(5, 200));
    REQUIRE_THAT(rx, Catch::Matchers::RangeEquals(std::vector<int32_t>{1, 2, 3}));
    REQUIRE(b.ctx->time() == 200);
    // The receive's event subscription is gone.
    REQUIRE_FALSE(EngineInspector::has_dynamic_subscribers(b.loop.rx.received()));
}

TEST_CASE("run_until throws TimeoutError and leaves the simulation usable", "[async][run_until]")
{
    AxisBench b;
    b.loop.rx.ready(1);
    const size_t n_processes = EngineInspector::process_count(*b.ctx);
    REQUIRE_THROWS_AS(b.ctx->run_until(b.loop.rx.receive(1), 100), TimeoutError);
    REQUIRE(b.ctx->time() == 100);
    REQUIRE(EngineInspector::process_count(*b.ctx) == n_processes);
    REQUIRE_FALSE(EngineInspector::has_dynamic_subscribers(b.loop.rx.received()));

    b.loop.tx.push_range(std::vector<int32_t>{42});
    REQUIRE_THAT(b.ctx->run_until(b.loop.rx.receive(1, 100)), Catch::Matchers::RangeEquals(std::vector<int32_t>{42}));
}

TEST_CASE("run_until throws when the simulation stalls", "[async][run_until]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    ctx->elaborate();
    // No clock: nothing will ever trigger s.
    REQUIRE_THROWS_WITH(ctx->run_until(s.change()), Catch::Matchers::ContainsSubstring("stalled"));
}

TEST_CASE("run_until waits on an event with a timeout", "[async][run_until]")
{
    AxisBench b;
    b.loop.rx.ready(1);
    REQUIRE_FALSE(b.ctx->run_until(b.loop.rx.received(), 100));
    REQUIRE(b.ctx->time() == 100);

    b.loop.tx.push_back(5);
    REQUIRE(b.ctx->run_until(b.loop.rx.received(), 100));
    REQUIRE(b.ctx->time() < 200);
    REQUIRE(b.loop.rx.size() == 1);
}

TEST_CASE("send resolves when the sink accepted the data", "[async][axis]")
{
    AxisBench b;
    std::vector<int32_t> data{1, 2, 3, 4};
    // Not ready: the send times out with the data still queued.
    REQUIRE_FALSE(b.ctx->run_until(b.loop.tx.send(data, 100)));
    REQUIRE(b.loop.tx.size() == 4);

    b.loop.rx.ready(1);
    REQUIRE(b.ctx->run_until(b.loop.tx.drain(1000)));
    REQUIRE(b.loop.tx.empty());
    REQUIRE_THAT(b.ctx->run_until(b.loop.rx.receive(4, 100)), Catch::Matchers::RangeEquals(data));
}

TEST_CASE("nested tasks return values and propagate exceptions", "[async][coro]")
{
    auto ctx = Context::create();
    NestedTasks m{"m"};
    ctx->elaborate();
    ctx->run(100);
    REQUIRE(m.value == 42);
    REQUIRE(m.value_time == 17);
    REQUIRE(m.error == "nested boom");
}

TEST_CASE("a wait with a timeout resumes on the event and drops the stale timeout", "[async][coro]")
{
    auto ctx = Context::create();
    Signal<uint8_t> e{"e"}, f{"f"};
    TimeoutWaiter m{"m", 100};
    m.e.bind(e);
    m.f.bind(f);
    ctx->elaborate();

    ctx->run(10);
    e.write(1);
    ctx->run(10);
    REQUIRE(m.results == std::vector<WaitResult>{WaitResult::Triggered});
    REQUIRE(m.wakeups == 1);

    // The timeout at t=100 must not resume the coroutine, which now waits on f.
    ctx->run(200);
    REQUIRE(m.wakeups == 1);
    f.write(1);
    ctx->run(10);
    REQUIRE(m.wakeups == 2);
}

TEST_CASE("a wait with a timeout resumes on the timeout and unsubscribes from the event", "[async][coro]")
{
    auto ctx = Context::create();
    Signal<uint8_t> e{"e"}, f{"f"};
    TimeoutWaiter m{"m", 100};
    m.e.bind(e);
    m.f.bind(f);
    ctx->elaborate();

    ctx->run(50);
    REQUIRE(m.wakeups == 0);
    ctx->run(60);
    REQUIRE(m.results == std::vector<WaitResult>{WaitResult::Timeout});
    REQUIRE(m.wakeups == 1);
    REQUIRE_FALSE(EngineInspector::has_dynamic_subscribers(e.change()));

    // e no longer wakes the coroutine.
    e.write(1);
    ctx->run(10);
    REQUIRE(m.wakeups == 1);
    f.write(1);
    ctx->run(10);
    REQUIRE(m.wakeups == 2);
}

TEST_CASE("a time wait ignores static sensitivity until it elapses", "[async][coro]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"};
    StaticAndTime m{"m"};
    m.s.bind(s);
    ctx->elaborate();

    ctx->run(10);
    s.write(1);
    ctx->run(10);
    REQUIRE(m.wakeups == 0);

    ctx->run(40);
    REQUIRE(m.wakeups == 1);

    // Static sensitivity is back.
    s.write(2);
    ctx->run(10);
    REQUIRE(m.wakeups == 2);
}

namespace
{
    // Waits zero time, twice, and counts how far it got.
    Task<int> wait_zero_twice(Context *context, int *resumed)
    {
        co_await context->wait(0);
        ++*resumed;
        co_await context->wait(0);
        ++*resumed;
        co_return 7;
    }

    Task<> run_wait_zero(Context *context, int *resumed)
    {
        co_await wait_zero_twice(context, resumed);
    }
} // namespace

TEST_CASE("a zero time wait resumes without advancing time", "[async][coro]")
{
    auto ctx = Context::create();
    int resumed = 0;
    ctx->register_coro_task(run_wait_zero(ctx.get(), &resumed), "waiter");
    ctx->elaborate();

    // run(0) settles the design at the current time, and a process that is due now is part of that.
    ctx->run(0);
    REQUIRE(resumed == 2);
    REQUIRE(ctx->time() == 0);
}

namespace
{
    // out follows in, one delta cycle later.
    DSPSIM_MODULE(Follower)
    {
        Input<int> in{"in"};
        Output<int> out{"out"};

        DSPSIM_CTOR(Follower)
        {
            DSPSIM_METHOD(eval)->always(in);
        }

        void eval()
        {
            out.write(in.read());
        }
    };

    // Writes a, then looks at a and b after each of two waits of no time.
    Task<> write_and_look(Context *context, Signal<int> *a, Signal<int> *b, std::vector<int> *seen)
    {
        a->write(5);
        co_await context->wait(0);
        seen->push_back(a->read());
        seen->push_back(b->read());
        co_await context->wait(0);
        seen->push_back(b->read());
    }

    // A method process that asks for another evaluation after no time, three times.
    DSPSIM_MODULE(Again)
    {
        Input<int> in{"in"};
        std::vector<int> seen;

        DSPSIM_CTOR(Again)
        {
            DSPSIM_METHOD(eval);
        }

        void eval()
        {
            seen.push_back(in.read());
            if (seen.size() < 3)
            {
                next_trigger(0);
            }
        }
    };
} // namespace

TEST_CASE("a zero time wait ends in the next delta cycle", "[async][coro]")
{
    auto ctx = Context::create();
    Signal<int> a{"a"}, b{"b"};
    Follower follower{"follower"};
    follower.in.bind(a);
    follower.out.bind(b);
    std::vector<int> seen;
    ctx->register_coro_task(write_and_look(ctx.get(), &a, &b, &seen), "watcher");
    ctx->elaborate();
    ctx->run(0);

    // One delta cycle after the write, a has its new value and the follower is evaluated, in the same
    // delta cycle as the watcher: b changes one delta cycle later.
    REQUIRE(seen == std::vector<int>{5, 0, 5});
    REQUIRE(ctx->time() == 0);
}

TEST_CASE("next_trigger of no time evaluates a method in the next delta cycle", "[async][process]")
{
    auto ctx = Context::create();
    Signal<int> a{"a"}, b{"b"};
    Follower follower{"follower"};
    follower.in.bind(a);
    follower.out.bind(b);
    Again again{"again"};
    again.in.bind(b);
    ctx->elaborate();
    a.write(9);
    ctx->run(0);

    // The value written before the run is committed before the first evaluation. The follower copies it in
    // the first delta cycle, so b has it in the second, which is when the process is evaluated again.
    REQUIRE(again.seen == std::vector<int>{0, 9, 9});
    REQUIRE(ctx->time() == 0);
}

TEST_CASE("run_until completes a task that waits zero time", "[async][run_until]")
{
    auto ctx = Context::create();
    ctx->elaborate();
    ctx->run(5);

    int resumed = 0;
    REQUIRE(ctx->run_until(wait_zero_twice(ctx.get(), &resumed)) == 7);
    REQUIRE(resumed == 2);
    REQUIRE(ctx->time() == 5);
}
