/*
    Events notified by user code: SensitivityEvent::notify() called from a process or from outside the
    simulation. (Signals notify their own events in the update phase; the other tests cover those.)
*/
#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

namespace
{
    Task<> notify_once(SensitivityEvent *event)
    {
        event->notify();
        co_return;
    }

    Task<> wait_once(Context *context, SensitivityEvent *event, int *woken)
    {
        co_await context->wait(*event);
        ++*woken;
    }
} // namespace

TEST_CASE("an event notified in an evaluation phase wakes a process that waits on it in the same phase", "[event]")
{
    // The notifier and the waiter are evaluated in the same delta cycle, in either order. The waiter must
    // be woken in both: the notification takes effect when the evaluation phase is over.
    for (bool notifier_first : {true, false})
    {
        auto ctx = Context::create();
        SensitivityEvent event{ctx.get()};
        int woken = 0;
        if (notifier_first)
        {
            ctx->register_coro_task(notify_once(&event), "notifier");
            ctx->register_coro_task(wait_once(ctx.get(), &event, &woken), "waiter");
        }
        else
        {
            ctx->register_coro_task(wait_once(ctx.get(), &event, &woken), "waiter");
            ctx->register_coro_task(notify_once(&event), "notifier");
        }
        ctx->elaborate();
        ctx->run(0);
        REQUIRE(woken == 1);
    }
}

TEST_CASE("an event notified in an evaluation phase wakes its static subscribers in the next delta cycle", "[event]")
{
    auto ctx = Context::create();
    SensitivityEvent event{ctx.get()};
    int evaluations = 0;
    bool happened = false;
    ctx->register_process_func([&]
                               {
                                   ++evaluations;
                                   happened = event.has_happened(); },
                               "subscriber")
        ->always(event)
        ->initialize(false);
    ctx->register_coro_task(notify_once(&event), "notifier");
    ctx->elaborate();
    ctx->run(0);

    REQUIRE(evaluations == 1);
    // The event has happened for the processes it wakes, as a signal's events have for theirs.
    REQUIRE(happened);
    REQUIRE_FALSE(event.has_happened());
}

TEST_CASE("an event notified between runs wakes its subscribers in the next run", "[event]")
{
    auto ctx = Context::create();
    SensitivityEvent event{ctx.get()};
    int evaluations = 0;
    ctx->register_process_func([&]
                               { ++evaluations; },
                               "subscriber")
        ->always(event)
        ->initialize(false);
    ctx->elaborate();
    ctx->run(0);
    REQUIRE(evaluations == 0);

    // Nothing else is scheduled: the notification alone has to start a delta cycle.
    event.notify();
    ctx->run(0);
    REQUIRE(evaluations == 1);

    ctx->run(0);
    REQUIRE(evaluations == 1);
}

TEST_CASE("an event can be notified again in the delta cycle in which it occurs", "[event]")
{
    // A process that the event wakes notifies it again: the event must occur a second time.
    auto ctx = Context::create();
    SensitivityEvent event{ctx.get()};
    int evaluations = 0;
    ctx->register_process_func([&]
                               {
                                   if (++evaluations < 3)
                                   {
                                       event.notify();
                                   } },
                               "subscriber")
        ->always(event)
        ->initialize(false);
    ctx->register_coro_task(notify_once(&event), "notifier");
    ctx->elaborate();
    ctx->run(0);
    REQUIRE(evaluations == 3);
}

TEST_CASE("an event notified twice in a delta cycle wakes a subscriber once", "[event]")
{
    auto ctx = Context::create();
    SensitivityEvent event{ctx.get()};
    int evaluations = 0;
    ctx->register_process_func([&]
                               { ++evaluations; },
                               "subscriber")
        ->always(event)
        ->initialize(false);
    ctx->register_coro_task(notify_once(&event), "first");
    ctx->register_coro_task(notify_once(&event), "second");
    ctx->elaborate();
    ctx->run(0);
    REQUIRE(evaluations == 1);
}

TEST_CASE("notifying an event without subscribers does nothing", "[event]")
{
    auto ctx = Context::create();
    SensitivityEvent event{ctx.get()};
    ctx->register_coro_task(notify_once(&event), "notifier");
    ctx->elaborate();
    ctx->run(0);
    ctx->run(10);
    REQUIRE(ctx->time() == 10);
}
