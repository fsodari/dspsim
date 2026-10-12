/*
    Processes that write many signals.

    wide:   a single process writes 10000 signals. The update phase dominates.
    matrix: 16 stages in series, each a process that writes 15 signals.
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/chain.h"
#include "models/chain_sc.h"
#include "models/wide.h"
#include "models/wide_sc.h"

using namespace benchmarks;

int sc_main(int argc, char *argv[])
{
    Suite suite{"wide"};

    {
        using T = double;
        constexpr int width = 10'000;
        const Params params{.iters = 600};
        suite.add("wide/n=10000", "method", params)
            .dspsim<ClockedTb<WideMethod<T, width>, T>>()
            .systemc<ClockedTbSC<WideMethodSC<T, width>, T>>();
        suite.add("wide/n=10000", "coro", params)
            .dspsim<ClockedTb<WideCoro<T, width>, T>>()
            .systemc<ClockedTbSC<WideThreadSC<T, width>, T>>();
    }
    {
        using T = uint32_t;
        constexpr int width = 15;
        constexpr int stages = 16;
        const Params params{.iters = 20'000};
        suite.add("matrix/16x15", "method", params)
            .dspsim<ClockedTb<Chain<WideMethod<T, width>, T>, T>>(stages)
            .systemc<ClockedTbSC<ChainSC<WideMethodSC<T, width>, T>, T>>(stages);
        suite.add("matrix/16x15", "coro", params)
            .dspsim<ClockedTb<Chain<WideCoro<T, width>, T>, T>>(stages)
            .systemc<ClockedTbSC<ChainSC<WideThreadSC<T, width>, T>, T>>(stages);
    }
    return suite.main(argc, argv);
}
