#include <dspsim/dspsim.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace dspsim;

TEST_CASE("elaborate detaches the context from the global factory", "[context]")
{
    auto a = Context::create("ctx_detach_a");
    Signal<uint8_t> sa{"sa"};
    REQUIRE(Context::obtain() == a);
    REQUIRE_FALSE(a->elaborated());

    a->elaborate();
    REQUIRE(a->elaborated());

    // Constructing models after elaboration is an error; the elaborated design is unaffected.
    REQUIRE_THROWS_AS(Signal<uint8_t>("sb"), ContextConstructionError);
    REQUIRE_THROWS_AS(Context::obtain(), ContextConstructionError);
    REQUIRE(sa.context() == a.get());
    REQUIRE(a->models().size() == 1);

    // A new design starts with create().
    auto b = Context::create("ctx_detach_b");
    Signal<uint8_t> sb{"sb"};
    REQUIRE(sb.context() == b.get());
    b->elaborate();
    REQUIRE_THROWS_AS(Context::obtain(), ContextConstructionError);
    Context::reset();
}

TEST_CASE("release only resets the active context", "[context]")
{
    auto a = Context::create("ctx_detach_c");
    a->elaborate();
    auto b = Context::create("ctx_detach_d");

    // a is no longer active, so releasing it must leave b active.
    a->release();
    REQUIRE(Context::obtain() == b);

    // With nothing active, there is no context to obtain.
    b->release();
    REQUIRE_THROWS_AS(Context::obtain(), ContextConstructionError);
    REQUIRE_THROWS_AS(Signal<uint8_t>("orphan"), ContextConstructionError);
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

namespace
{
    // Counts posedges of a clock.
    DSPSIM_MODULE(EdgeCount)
    {
        Input<uint8_t> clk{"clk"};
        int count = 0;

        DSPSIM_CTOR(EdgeCount)
        {
            DSPSIM_METHOD(eval)->always(clk.pos())->initialize(false);
        }

        void eval() { ++count; }
    };
} // namespace

TEST_CASE("other threads wait for the context under construction", "[context][thread]")
{
    using namespace std::chrono_literals;

    auto a = Context::create("ctx_thread_main");
    std::atomic<bool> created = false;
    bool obtain_threw = false;
    std::shared_ptr<Context> other;

    std::thread t([&]
                  {
                      // The context under construction belongs to the main thread.
                      try
                      {
                          Context::obtain();
                      }
                      catch (const ContextConstructionError &)
                      {
                          obtain_threw = true;
                      }
                      other = Context::create("ctx_thread_other");
                      created = true;
                      Signal<uint8_t> s{"s"};
                      other->elaborate(); });

    // The other thread blocks until this context is elaborated.
    std::this_thread::sleep_for(100ms);
    REQUIRE_FALSE(created);
    REQUIRE(a->constructing());
    a->elaborate();
    REQUIRE_FALSE(a->constructing());
    t.join();

    REQUIRE(created);
    REQUIRE(obtain_threw);
    REQUIRE(other->name() == "ctx_thread_other");
    REQUIRE(other->elaborated());
    REQUIRE(other->models().size() == 1);
}

TEST_CASE("threads construct designs one at a time and simulate in parallel", "[context][thread]")
{
    constexpr int n_threads = 8;
    std::vector<int> counts(n_threads, -1);
    // Catch2 assertions aren't thread safe, so errors are collected and checked on the main thread.
    std::vector<std::string> errors(n_threads);
    std::vector<std::thread> threads;

    for (int i = 0; i < n_threads; ++i)
    {
        threads.emplace_back([i, &counts, &errors]
                             {
                                 try
                                 {
                                     auto ctx = Context::create("ctx_parallel_" + std::to_string(i));
                                     Clock clk{"clk", 10};
                                     EdgeCount counter{"counter"};
                                     counter.clk.bind(clk);
                                     ctx->elaborate();
                                     ctx->run(1000);
                                     counts[i] = counter.count;
                                 }
                                 catch (const std::exception &e)
                                 {
                                     errors[i] = e.what();
                                 } });
    }
    for (auto &t : threads)
    {
        t.join();
    }

    for (int i = 0; i < n_threads; ++i)
    {
        INFO("thread " << i);
        REQUIRE(errors[i] == "");
        REQUIRE(counts[i] == 100);
    }
}
