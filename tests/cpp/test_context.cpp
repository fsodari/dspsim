#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

TEST_CASE("elaborate detaches the context from the global factory", "[context]")
{
    auto a = Context::create("ctx_detach_a");
    Signal<uint8_t> sa{"sa"};
    REQUIRE(Context::obtain() == a);
    REQUIRE_FALSE(a->elaborated());

    a->elaborate();
    REQUIRE(a->elaborated());

    // New models go to a new context; the elaborated design is unaffected.
    Signal<uint8_t> sb{"sb"};
    REQUIRE(sb.context() != a.get());
    REQUIRE(sa.context() == a.get());
    REQUIRE(a->models().size() == 1);
    Context::reset();
}

TEST_CASE("detach only resets the active context", "[context]")
{
    auto a = Context::create("ctx_detach_c");
    a->elaborate();
    auto b = Context::create("ctx_detach_d");

    // a is no longer active, so detaching it must leave b active.
    a->detach();
    REQUIRE(Context::obtain() == b);

    b->detach();
    REQUIRE(Context::obtain() != b);
    Context::reset();
}

TEST_CASE("processes belong to the registering context", "[context][process]")
{
    auto a = Context::create("ctx_process_owner");
    a->elaborate();
    // Registering after elaboration must not pick up a different global context.
    auto *p = a->register_process_func([] {}, "late");
    REQUIRE(p->context() == a.get());
    Context::reset();
}

TEST_CASE("create refuses to replace an unelaborated context", "[context]")
{
    auto a = Context::create("ctx_unelaborated");
    Signal<uint8_t> s{"s"};
    REQUIRE_THROWS_AS(Context::create("ctx_replacement"), ContextConstructionError);
    // The unelaborated context is still active.
    REQUIRE(Context::obtain() == a);

    // reset() explicitly discards it.
    Context::reset();
    auto b = Context::create("ctx_replacement");
    REQUIRE(Context::obtain() == b);
    b->elaborate();
}

TEST_CASE("models get unique ids and default names on construction", "[context][model]")
{
    auto ctx = Context::create("ctx_model_ids");
    Signal<uint8_t> a{};
    Signal<uint8_t> b{};
    REQUIRE(a.id() != b.id());
    REQUIRE(a.name() == "signal" + std::to_string(a.id()));
    REQUIRE(b.name() == "signal" + std::to_string(b.id()));
    REQUIRE(a.hier_name() == "root." + a.name());
    ctx->elaborate();
}

TEST_CASE("contexts with the same name can coexist", "[context]")
{
    auto a = Context::create("ctx_same_name");
    a->elaborate();
    auto b = Context::create("ctx_same_name");
    REQUIRE(a->name() == b->name());
    REQUIRE(a->id() != b->id());
    b->elaborate();
}
