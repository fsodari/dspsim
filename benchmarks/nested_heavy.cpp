#include <dspsim/dspsim.h>
#include <systemc>
#include "utils.h"
#include "nested.h"
#include "heavy.h"

using namespace benchmarks;
using namespace dspsim;

int sc_main(int argc, char *argv[])
{
    using T = uint32_t;
    size_t n_iter = argc > 1 ? std::stoul(argv[1]) : 100000;
    size_t run_step = argc > 2 ? std::stoul(argv[2]) : 10;
    constexpr size_t n_heavy_calcs = 10;
    constexpr size_t depth = 100; // Example depth for the nested module

    auto dspsim_result = dspsim_runner<NestedComb<HeavyWire<T, n_heavy_calcs>, T, depth>, T>(n_iter, run_step);
    auto systemc_result = systemc_runner<NestedCombSC<HeavyWireSC<T, n_heavy_calcs>, T, depth>, T>(n_iter, run_step);
    return 0;
}
