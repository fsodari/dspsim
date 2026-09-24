#include <dspsim/dspsim.h>
#include <spdlog/spdlog.h>
#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

// Anonymous namespace: avoids ODR collisions with same-named classes in other test translation units.
namespace
{
    // Recursively nests a submodule named "child" Depth levels deep. Each level also owns an
    // Input, Output, and internal Signal, so the hierarchy includes port/signal models, not just modules.
    template <int Depth>
    class Nested : public Module
    {
    public:
        Input<int> i{"i"};
        Output<int> o{"o"};
        Signal<int> internal{"internal"};
        Nested<Depth - 1> child{"child"};

        Nested(ModuleName name) : Module(name)
        {
            child.i.bind(i);
            child.o.bind(internal);

            DSPSIM_METHOD(&Nested::eval_)->always(i, internal);
        }

        void eval_()
        {
            o.write(internal.read());
        }
    };

    template <>
    class Nested<0> : public Module
    {
    public:
        Input<int> i{"i"};
        Output<int> o{"o"};

        Nested(ModuleName name) : Module(name)
        {
            DSPSIM_METHOD(&Nested::eval_)->always(i);
        }

        void eval_()
        {
            o.write(i.read() + 1);
        }
    };

} // namespace

TEST_CASE("print_hierarchy doesn't segfault on nested modules", "[print_hierarchy]")
{
    auto ctx = Context::create();
    ctx->set_log_level("warn");

    Signal<int> a{"a"};
    Signal<int> b{"b"};
    Signal<int> a2{"a2"};
    Signal<int> b2{"b2"};

    Nested<4> top{"top"};
    Nested<4> top2{"top2"};

    top.i.bind(a);
    top.o.bind(b);
    top2.i.bind(a2);
    top2.o.bind(b2);

    ctx->elaborate();

    // Roots: the 4 free-standing signals plus the 2 top-level modules.
    REQUIRE(ctx->children().size() == 6);
    REQUIRE(ctx->children(&top).size() == 4);
    REQUIRE(ctx->children(&top.child).size() == 4);
    REQUIRE(ctx->children(&top.child.child).size() == 4);
    REQUIRE(ctx->children(&top.child.child.child).size() == 4);
    REQUIRE(ctx->children(&top.child.child.child.child).size() == 2);

    REQUIRE_NOTHROW(ctx->print_hierarchy());

    // Sanity check that the ports/signals actually wired up correctly through all 4 levels.
    ctx->run(0);
    REQUIRE(b.read() == 1);
    a.write(5);
    ctx->run(0);
    REQUIRE(b.read() == 6);
}
