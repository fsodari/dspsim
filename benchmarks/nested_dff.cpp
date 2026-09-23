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
    // Optional 3rd arg to isolate one engine for profiling: "dspsim", "systemc", or "both" (default).
    std::string which = argc > 3 ? argv[3] : "both";

    constexpr size_t depth = 200; // Example depth for the nested module

    if (which == "dspsim" || which == "both")
    {
        auto dspsim_result = dspsim_runner<NestedComb<SimpleDff<double>, double, depth>, double>(n_iter, run_step);
    }
    if (which == "systemc" || which == "both")
    {
        auto systemc_result = systemc_runner<NestedCombSC<SimpleDffSC<double>, double, depth>, double>(n_iter, run_step);
    }
    return 0;
}
