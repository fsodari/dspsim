#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

#include <optional>

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
    REQUIRE(root_signal.parent() == nullptr);
    ctx->elaborate();
}

TEST_CASE("ModuleName copies share one scope", "[module_name]")
{
    auto ctx = Context::create();
    std::optional<ModuleName> name{"m"};
    Module m{*name};
    {
        ModuleName copy = *name;
        REQUIRE(copy.name() == "m");
    }
    // The copy is gone, but the original still holds the scope open.
    Signal<uint8_t> inner{"inner"};
    REQUIRE(inner.parent() == &m);
    REQUIRE(inner.hier_name() == "root.m.inner");

    // The scope ends with the last copy: new models are at the root again.
    name.reset();
    Signal<uint8_t> outer{"outer"};
    REQUIRE(outer.parent() == nullptr);
    ctx->elaborate();
}

TEST_CASE("ModuleName::end is explicit and idempotent", "[module_name]")
{
    auto ctx = Context::create();
    ModuleName name{"m"};
    Module m{name};
    name.end();
    Signal<uint8_t> after_end{"after_end"};
    REQUIRE(after_end.parent() == nullptr);
    name.end();
    Signal<uint8_t> outer{"outer"};
    REQUIRE(outer.parent() == nullptr);
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
    Signal<uint8_t> in_parent{"in_parent"};
    REQUIRE(in_parent.parent() == &parent);
    parent_name.end();
    Signal<uint8_t> at_root{"at_root"};
    REQUIRE(at_root.parent() == nullptr);
    ctx->elaborate();
}
