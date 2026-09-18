#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <dspsim/signal.h>
#include <dspsim/clock.h>

#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

class SomeModule : public Module
{
public:
    Input<uint8_t> a;
    Input<uint8_t> b;
    Output<uint8_t> c;

    SomeModule(ModuleName name, Signal<uint8_t> &a_, Signal<uint8_t> &b_, Signal<uint8_t> &c_)
        : Module(name),
          a("a", a_),
          b("b", b_),
          c("c", c_)
    {
        always << a << b;
    }

    void eval() override
    {
        // std::cout << "SomeModule eval()" << std::endl;
        context()->logger->debug("SomeModule eval(), time: {}", context()->time());
        c.write(a.read() + b.read());
    }
};

class SyncModel : public Module
{
public:
    Input<uint8_t> clk;
    Input<uint8_t> d;
    Output<uint8_t> q;

    SomeModule some_module;

    // SyncModel(ModuleName name) : Module(name)
    // DSPSIM_CTOR(SyncModel) : some_module("some_module_sync_model")
    SyncModel(ModuleName name, Signal<uint8_t> &clk_, Signal<uint8_t> &d_, Signal<uint8_t> &q_)
        : Module(name),
          clk("clk", clk_),
          d("d", d_),
          q("q", q_),
          some_module("some_module_sync_model", d, d, q)
    {
        always << clk.pos();
    }

    void eval() override
    {
        if (clk.read() == 1)
        {
            context()->logger->debug("SyncModel eval() on posedge, time: {}", context()->time());
            q.write(d.read());
        }
        else
        {
            context()->logger->error("SyncModel eval() on negedge, time: {}", context()->time());
        }
    }
};
/*
    top_d -> Input_d -> internal_d
*/

TEST_CASE("test_hierarchy")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::warn);
    Clock clk_top{"clk_top", 10};
    Signal<uint8_t> d_top{"d_top"};
    Signal<uint8_t> q_top{"q_top"};

    // SyncModel sync_model{"sync_model"};
    auto sync_model = SyncModel("sync_model", clk_top, d_top, q_top);

    ctx->logger->info("Here");

    Signal<uint8_t> foo{"foo"};

    ctx->elaborate();

    for (auto m : ctx->models())
    {
        ctx->logger->debug("Model: {}, hier_name: {}", m->name(), m->hier_name());
    }
}

// int main(int argc, char **argv)
// {
//     auto ctx = Context::obtain();

//     // Signal<int> ai1("ai1").init(0);
//     auto ai1 = Signal<int>{"ai1"};
//     Signal<int> ai2{"ai2"};
//     Signal<int> ao{"ao"};

//     Signal<int> bi1{"bi1"};
//     Signal<int> bi2{"bi2"};
//     Signal<int> bo{"bo"};

//     Signal<int> ci1{"ci1"};
//     Signal<int> ci2{"ci2"};
//     Signal<int> co{"co"};

//     Signal<int> di1{"di1"};
//     Signal<int> di2{"di2"};
//     Signal<int> do_{"do"};

//     Signal<int> eo{"eo"};
//     Signal<int> fo{"fo"};

//     Signal<int> go{"go"};

//     SomeModule moda{"moda", ai1, ai2, ao};
//     // moda.a.bind(&ai1);
//     // moda.b.bind(&ai2);
//     // moda.c.bind(&ao);

//     SomeModule modb{"modb", bi1, bi2, bo};
//     // modb.a.bind(&bi1);
//     // modb.b.bind(&bi2);
//     // modb.c.bind(&bo);

//     SomeModule modc{"modc", ci1, ci2, co};

//     SomeModule modd{"modd", di1, di2, do_};

//     SomeModule mode{"mode", ao, bo, eo};

//     SomeModule modf{"modf", co, do_, fo};

//     SomeModule modg{"modg", eo, fo, go};

//     auto n_iter = ctx->delta_cycle();
//     std::cout << "Number of delta cycles: " << n_iter << std::endl;
//     std::cout << "go: " << go.read() << std::endl;

//     // Write to signals.
//     ai1.write(2);
//     ai2.write(1);
//     bi1.write(1);
//     bi2.write(1);
//     ci1.write(1);
//     ci2.write(1);
//     di1.write(1);
//     di2.write(1);

//     n_iter = ctx->delta_cycle();
//     std::cout << "Number of delta cycles: " << n_iter << std::endl;
//     std::cout << "go: " << go.read() << std::endl;

//     return 0;
// }
