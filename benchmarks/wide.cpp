#include <dspsim/dspsim.h>
#include <systemc>
#include "utils.h"
#include "wide.h"

using namespace benchmarks;
using namespace dspsim;

int sc_main(int argc, char *argv[])
{
    using T = double;
    size_t n_iter = argc > 1 ? std::stoul(argv[1]) : 1000000;
    size_t run_step = argc > 2 ? std::stoul(argv[2]) : 10;

    constexpr size_t width = 500; // Example width for the wide module

    auto dspsim_result = dspsim_runner<Wide<T, width>, T>(n_iter, run_step);
    auto systemc_result = systemc_runner<WideSC<T, width>, T>(n_iter, run_step);
    return 0;
}
