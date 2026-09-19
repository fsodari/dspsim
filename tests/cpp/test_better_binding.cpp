#include <dspsim/dspsim.h>
#include <spdlog/spdlog.h>
#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

// Anonymous namespace: avoids ODR collisions with same-named classes in other test translation units.
namespace
{

    class Child : public Module
    {
    public:
        Input<int> i{"i"};
        Output<int> o{"o"};

        Child(ModuleName name) : Module(name)
        {
            always << i;
        }

        void eval() override
        {
            o.write(i.read() + 1);
        }
    };

    class Parent : public Module
    {
    public:
        // Order in class shouldn't matter.
        Child child{"child"};

        Input<int> i{"i"};
        Output<int> o{"o"};

        Parent(ModuleName name) : Module(name)
        {
            // Bind in the constructor.
            child.i.bind(i);
            child.o.bind(o);
        }
    };

    class Top : public Module
    {
    public:
        // Initialize ports here, instead of in the constructor.
        // Don't require binding immediately.
        Input<int> i{"i"};
        Output<int> o{"o"};

        Parent parent1{"parent1"};
        Parent parent2{"parent2"};

        // Allow signals to be declared in any order within the module.
        Signal<int> p1_internal{"p1_internal"};
        Signal<int> p2_internal{"p2_internal"};

        Top(ModuleName name) : Module(name)
        {
            always << i << p1_internal << p2_internal;

            parent1.i.bind(i);
            parent1.o.bind(p1_internal);
            parent2.i.bind(i);
            parent2.o.bind(p2_internal);
        }
        void eval() override
        {
            o.write(p1_internal.read() + p2_internal.read());
        }
    };

} // namespace

TEST_CASE("Better binding test", "[better_binding]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::trace);

    Signal<int> a{"a"};
    Signal<int> b{"b"};

    Top top{"top"};

    // Allow binding after module construction.
    top.i.bind(a);
    top.o.bind(b);

    // Elaboration will finalize the construction.
    ctx->elaborate();

    ctx->eval();
    REQUIRE(b.read() == 2);
    a.write(5);
    ctx->eval();
    REQUIRE(b.read() == 12);
}