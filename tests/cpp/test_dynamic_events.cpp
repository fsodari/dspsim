#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

namespace
{
    // Waits on a or b, then on c.
    DSPSIM_MODULE(WaitAny)
    {
        Input<uint8_t> a{"a"};
        Input<uint8_t> b{"b"};
        Input<uint8_t> c{"c"};
        int wakeups = 0;

        DSPSIM_CTOR(WaitAny)
        {
            DSPSIM_CORO(body);
        }

        Task<> body()
        {
            co_await wait({a.change(), b.change()});
            ++wakeups;
            co_await wait(c.change());
            ++wakeups;
        }
    };

    // First waits on any input with next_trigger("*"), then only on t.
    DSPSIM_MODULE(TriggerStar)
    {
        Input<uint8_t> a{"a"};
        Input<uint8_t> b{"b"};
        Input<uint8_t> t{"t"};
        int evals = 0;

        DSPSIM_CTOR(TriggerStar)
        {
            DSPSIM_METHOD(eval);
        }

        void eval()
        {
            ++evals;
            if (evals == 1)
            {
                next_trigger("*");
            }
            else
            {
                next_trigger(t.change());
            }
        }
    };

    // Statically sensitive to s, but waits dynamically on e.
    DSPSIM_MODULE(StaticAndDynamic)
    {
        Input<uint8_t> s{"s"};
        Input<uint8_t> e{"e"};
        int wakeups = 0;

        DSPSIM_CTOR(StaticAndDynamic)
        {
            DSPSIM_CORO(body)->always(s.change());
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

TEST_CASE("waiting on several events unsubscribes from the others", "[coro][dynamic]")
{
    auto ctx = Context::create();
    Signal<uint8_t> a{"a"}, b{"b"}, c{"c"};
    WaitAny m{"m"};
    m.a.bind(a);
    m.b.bind(b);
    m.c.bind(c);
    ctx->elaborate();
    ctx->run(1);
    REQUIRE(m.wakeups == 0);

    a.write(1);
    ctx->run(1);
    REQUIRE(m.wakeups == 1);
    REQUIRE(b.change().dynamic_subscribers().empty());

    // The process now waits on c only, so b must not wake it.
    b.write(1);
    ctx->run(1);
    REQUIRE(m.wakeups == 1);

    c.write(1);
    ctx->run(1);
    REQUIRE(m.wakeups == 2);
}

TEST_CASE("next_trigger(\"*\") unsubscribes from the other inputs", "[process][dynamic]")
{
    auto ctx = Context::create();
    Signal<uint8_t> a{"a"}, b{"b"}, t{"t"};
    TriggerStar m{"m"};
    m.a.bind(a);
    m.b.bind(b);
    m.t.bind(t);
    ctx->elaborate();
    ctx->run(1);
    REQUIRE(m.evals == 1);

    a.write(1);
    ctx->run(1);
    REQUIRE(m.evals == 2);

    b.write(1);
    ctx->run(1);
    REQUIRE(m.evals == 2);

    t.write(1);
    ctx->run(1);
    REQUIRE(m.evals == 3);
}

TEST_CASE("a coroutine waiting on a dynamic event ignores its static sensitivity", "[coro][dynamic]")
{
    auto ctx = Context::create();
    Signal<uint8_t> s{"s"}, e{"e"};
    StaticAndDynamic m{"m"};
    m.s.bind(s);
    m.e.bind(e);
    ctx->elaborate();
    ctx->run(1);

    s.write(1);
    ctx->run(1);
    REQUIRE(m.wakeups == 0);

    e.write(1);
    ctx->run(1);
    REQUIRE(m.wakeups == 1);
}
