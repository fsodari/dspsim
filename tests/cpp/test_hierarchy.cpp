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
        Input<uint8_t> a;
        Input<uint8_t> b;
        Output<uint8_t> c;

        SomeModule(ModuleName name, Signal<uint8_t> &a_, Signal<uint8_t> &b_, Signal<uint8_t> &c_)
            : Module(name),
              a("a", a_),
              b("b", b_),
              c("c", c_)
        {
            DSPSIM_METHOD(&SomeModule::eval_)->always("*");
        }

        void eval_()
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
            DSPSIM_METHOD(&SyncModel::eval_)->always(clk.pos());
        }

        void eval_()
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
    auto sync_model = SyncModel("sync_model", clk_top, d_top, q_top);

    ctx->logger->info("Here");

    Signal<uint8_t> foo{"foo"};

    ctx->elaborate();

    for (auto m : ctx->models())
    {
        ctx->logger->debug("Model: {}, hier_name: {}", m->name(), m->hier_name());
    }
}
