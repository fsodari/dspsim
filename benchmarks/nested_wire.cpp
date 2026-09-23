#include <dspsim/dspsim.h>
#include <systemc>
#include "utils.h"
#include "nested.h"
#include "simple.h"

using namespace benchmarks;
using namespace dspsim;

int sc_main(int argc, char *argv[])
{
    size_t n_iter = argc > 1 ? std::stoul(argv[1]) : 1000000;
    size_t run_step = argc > 2 ? std::stoul(argv[2]) : 10;

    constexpr size_t depth = 100; // Example depth for the nested module

    auto dspsim_result = dspsim_runner<NestedComb<SimpleWire<double>, double, depth>, double>(n_iter, run_step);
    auto systemc_result = systemc_runner<NestedCombSC<SimpleWireSC<double>, double, depth>, double>(n_iter, run_step);
    return 0;
}
