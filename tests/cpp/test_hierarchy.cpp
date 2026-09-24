#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <dspsim/port.h>
#include <dspsim/signal.h>
#include <dspsim/clock.h>

#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>

using namespace dspsim;
namespace
{
    class SomeModule : public Module
    {
    public:
        Input<uint8_t> a{"a"};
        Input<uint8_t> b{"b"};
        Output<uint8_t> c{"c"};

        SomeModule(ModuleName name)
            : Module(name)
        {
            DSPSIM_METHOD(&SomeModule::eval_)->always("*");
        }

        void eval_()
        {
            context()->logger->debug("SomeModule eval(), time: {}", context()->time());
            c.write(a.read() + b.read());
        }
    };

    class SyncModel : public Module
    {
    public:
        Input<uint8_t> clk{"clk"};
        Input<uint8_t> d{"d"};
        Output<uint8_t> q{"q"};

        Signal<uint8_t> sig{"sig"};

        SomeModule some_module{"some_module"};

        SyncModel(ModuleName name)
            : Module(name)
        {
            some_module.a.bind(clk);
            some_module.b.bind(d);
            some_module.c.bind(sig);
            DSPSIM_METHOD(&SyncModel::eval_)->always(clk.pos());
        }

        void eval_()
        {
            if (clk.posedge())
            {
                context()->logger->debug("SyncModel eval() on posedge, time: {}", context()->time());
                q.write(sig.read());
            }
            else
            {
                context()->logger->error("SyncModel eval() on negedge, time: {}", context()->time());
            }
        }
    };
}
/*
    top_d -> Input_d -> internal_d
*/

TEST_CASE("test_hierarchy", "[hierarchy]")
{
    auto ctx = Context::create();
    ctx->logger->set_level(spdlog::level::warn);
    Clock clk_top{"clk_top", 10};
    Signal<uint8_t> d_top{"d_top"};
    Signal<uint8_t> q_top{"q_top"};

    // SyncModel sync_model{"sync_model"};
    auto sync_model = SyncModel("sync_model");
    sync_model.clk.bind(clk_top);
    sync_model.d.bind(d_top);
    sync_model.q.bind(q_top);

    ctx->logger->info("Here");

    Signal<uint8_t> foo{"foo"};

    ctx->elaborate();

    for (auto m : ctx->models())
    {
        ctx->logger->debug("Model: {}, hier_name: {}", m->name(), m->hier_name());
    }
}
