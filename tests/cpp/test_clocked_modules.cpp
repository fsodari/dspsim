#include <dspsim/dspsim.h>
#include <spdlog/spdlog.h>
#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

// Anonymous namespace: avoids ODR collisions with same-named classes in other test translation units.
namespace
{
    class SomeModule : public Module
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<int> i{"i"};
        Output<int> o{"o"};

        SomeModule(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(&SomeModule::eval_);
            always << clk.pos();

            // Prevent initial eval step
            dont_initialize();
        }

        void eval_()
        {
            if (clk.posedge())
            {
                o.write(i.read());
            }
            else
            {
                FAIL("This should never eval unless there is a posedge event on clk");
            }
        }
    };

    class MultiClockSensitive : public Module
    {
    public:
        Input<uint8_t> clk1{"clk1"};
        Input<uint8_t> clk2{"clk2"};
        Input<int> i{"i"};
        Output<int> o{"o"};

        int clk1_counts = 0;
        int clk2_counts = 0;

        MultiClockSensitive(ModuleName name) : Module(name)
        {
            // Register the eval_ method with the simulation kernel.
            DSPSIM_METHOD(&MultiClockSensitive::eval_);
            always << clk1.pos() << clk2.pos();

            // Prevent initial eval step
            dont_initialize();
        }

        void eval_()
        {
            context()->logger->info("multi.eval_()");
            if (clk1.posedge())
            {
                clk1_counts++;
                context()->logger->info("multi.eval_(), clk1_posedge,clk1_counts: {}, clk2_counts: {}", clk1_counts, clk2_counts);
            }
            if (clk2.posedge())
            {
                clk2_counts++;
                context()->logger->info("multi.eval_(), clk2_posedge,clk1_counts: {}, clk2_counts: {}", clk1_counts, clk2_counts);
            }
        }
    };

} // namespace

TEST_CASE("Clocked module", "[clock][clock1]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::err);

    Clock clk{"clk", 10};
    Signal<int> a{"a"};
    Signal<int> b{"b"};

    SomeModule top{"top"};

    // Allow binding after module construction.
    top.clk.bind(clk);
    top.i.bind(a);
    top.o.bind(b);

    // Elaboration will finalize the construction.
    ctx->elaborate();

    ctx->eval();
    REQUIRE(b.read() == a.read());
    a.write(5);
    ctx->eval();
    REQUIRE(b.read() != a.read());
    ctx->run(10);
    REQUIRE(b.read() == a.read());
    ctx->run(100);

    for (int x = 0; x < 10; ++x)
    {
        a.write(x);
        ctx->run(10);
        REQUIRE(b.read() == a.read());
    }
}

TEST_CASE("multi clocks", "[clock][clock2]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::trace);

    Clock clk1{"clk1", 6};
    Clock clk2{"clk2", clk1.period() * 2};
    Signal<int> a{"a"};
    Signal<int> b{"b"};

    MultiClockSensitive top{"top"};

    // Allow binding after module construction.
    top.clk1.bind(clk1);
    top.clk2.bind(clk2);
    top.i.bind(a);
    top.o.bind(b);

    // Elaboration will finalize the construction.
    ctx->elaborate();

    // Clocks will update on first delta cycle.
    ctx->eval();
    REQUIRE(top.clk1_counts == 1);
    REQUIRE(top.clk2_counts == 1);

    // for (int x = 1; x < 100; x++)
    // {
    //     a.write(x);
    //     ctx->run(clk1.period());
    //     REQUIRE(top.clk1_counts == x + 1);
    //     REQUIRE(top.clk2_counts == x / 2 + 1);
    // }
}