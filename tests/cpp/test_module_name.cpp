#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

namespace
{
    DSPSIM_MODULE(NameBase)
    {
        Signal<uint8_t> a{"a"};
        DSPSIM_CTOR(NameBase) {}
    };

    // Derived from a module, passing its ModuleName on by value.
    struct NameDerived : public NameBase
    {
        Signal<uint8_t> b{"b"};
        NameDerived(ModuleName name) : NameBase(name) {}
    };

    // One more level of inheritance.
    struct NameDerived2 : public NameDerived
    {
        Signal<uint8_t> c{"c"};
        NameDerived2(ModuleName name) : NameDerived(name) {}
    };

    DSPSIM_MODULE(NameTop)
    {
        NameDerived2 d{"d"};
        Signal<uint8_t> after_d{"after_d"};
        DSPSIM_CTOR(NameTop) {}
    };
} // namespace

TEST_CASE("derived modules keep the hierarchy open until the most derived ctor returns", "[module_name]")
{
    auto ctx = Context::create();
    NameTop top{"top"};
    Signal<uint8_t> root_signal{"root_signal"};

    REQUIRE(top.d.a.hier_name() == "root.top.d.a");
    REQUIRE(top.d.b.hier_name() == "root.top.d.b");
    REQUIRE(top.d.c.hier_name() == "root.top.d.c");
    REQUIRE(top.after_d.hier_name() == "root.top.after_d");
    REQUIRE(root_signal.hier_name() == "root.root_signal");
    REQUIRE(ctx->_active_module_stack.empty());
    REQUIRE(ctx->_active_module_name_stack.empty());
    ctx->elaborate();
}

TEST_CASE("ModuleName copies share one scope", "[module_name]")
{
    auto ctx = Context::create();
    {
        ModuleName name{"m"};
        Module m{name};
        {
            ModuleName copy = name;
            REQUIRE(copy.name() == "m");
        }
        // The copy is gone, but the original still holds the scope open.
        REQUIRE(ctx->_active_module() == &m);
        Signal<uint8_t> inner{"inner"};
        REQUIRE(inner.hier_name() == "root.m.inner");
    }
    REQUIRE(ctx->_active_module() == nullptr);
    ctx->elaborate();
}

TEST_CASE("ModuleName::end is explicit and idempotent", "[module_name]")
{
    auto ctx = Context::create();
    ModuleName name{"m"};
    Module m{name};
    name.end();
    REQUIRE(ctx->_active_module() == nullptr);
    REQUIRE(ctx->_active_module_name_stack.empty());
    name.end();
    Signal<uint8_t> outer{"outer"};
    REQUIRE(outer.hier_name() == "root.outer");
    ctx->elaborate();
}

TEST_CASE("ending a scope without a module leaves the parent active", "[module_name]")
{
    auto ctx = Context::create();
    ModuleName parent_name{"parent"};
    Module parent{parent_name};
    {
        // No Module is constructed in this scope, so ending it must not pop the parent.
        ModuleName orphan{"orphan"};
    }
    REQUIRE(ctx->_active_module() == &parent);
    parent_name.end();
    REQUIRE(ctx->_active_module() == nullptr);
    ctx->elaborate();
}
