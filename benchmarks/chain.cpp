/*
    Chains of stages on one clock.

    dff:  flip-flops in series. A clock edge makes every stage runnable at once: many evaluations in one
          delta cycle, and one event (the clock edge) with many subscribers.
    wire: wires in series. An input change ripples through the chain one stage per delta cycle: many delta
          cycles with a single evaluation each.

    Each design comes in several process kinds, which shows what a kind costs in each engine.
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/chain.h"
#include "models/chain_sc.h"

#include <string>

using namespace benchmarks;

namespace
{
    // Add one kind of a chain of `stages` stages: Stage and StageSC are the dspsim and SystemC stage.
    template <template <typename> class Stage, template <typename> class StageSC, typename T, uint64_t STEP = clock_period>
    void add_chain(Suite &suite, const std::string &group, const std::string &kind, int stages, Params params)
    {
        Case &c = suite.add(group, kind, params);
        c.dspsim<ClockedTb<Chain<Stage<T>, T>, T, STEP>>(stages);
        c.systemc<ClockedTbSC<ChainSC<StageSC<T>, T>, T, STEP>>(stages);
    }
}

int sc_main(int argc, char *argv[])
{
    Suite suite{"chain"};

    {
        // The warm-up has to fill the chain: until then, the last stages do not change.
        using T = uint32_t;
        const Params params{.iters = 8'000, .warmup = 1'000};
        add_chain<DffMethod, DffMethodSC, T>(suite, "dff/n=500", "method", 500, params);
        add_chain<DffMethodDynamic, DffMethodDynamicSC, T>(suite, "dff/n=500", "method_dynamic", 500, params);
        add_chain<DffCoro, DffThreadSC, T>(suite, "dff/n=500", "coro", 500, params);
        add_chain<DffCoroDynamic, DffThreadDynamicSC, T>(suite, "dff/n=500", "coro_dynamic", 500, params);
    }
    {
        using T = uint32_t;
        const Params params{.iters = 40'000};
        add_chain<DffMethod, DffMethodSC, T>(suite, "dff/n=100", "method", 100, params);
        add_chain<DffCoro, DffThreadSC, T>(suite, "dff/n=100", "coro", 100, params);
    }
    {
        // A small design, and a step that is not a multiple of the clock period: steps end between clock
        // edges, on a falling edge and on a rising edge. The cost of entering and leaving run() dominates.
        using T = uint32_t;
        const Params params{.iters = 500'000};
        add_chain<DffMethod, DffMethodSC, T, 7>(suite, "dff/n=2,step=7", "method", 2, params);
        add_chain<DffCoro, DffThreadSC, T, 7>(suite, "dff/n=2,step=7", "coro", 2, params);
    }
    {
        using T = double;
        const Params params{.iters = 15'000};
        add_chain<WireMethod, WireMethodSC, T>(suite, "wire/n=100", "method", 100, params);
        add_chain<WireMethodDynamic, WireMethodDynamicSC, T>(suite, "wire/n=100", "method_dynamic", 100, params);
        add_chain<WireCoro, WireThreadSC, T>(suite, "wire/n=100", "coro", 100, params);
        add_chain<WireCoroDynamic, WireThreadDynamicSC, T>(suite, "wire/n=100", "coro_dynamic", 100, params);
    }
    {
        using T = uint32_t;
        const Params params{.iters = 500'000};
        add_chain<WireMethod, WireMethodSC, T>(suite, "wire/n=1", "method", 1, params);
        add_chain<WireCoro, WireThreadSC, T>(suite, "wire/n=1", "coro", 1, params);
    }
    return suite.main(argc, argv);
}
