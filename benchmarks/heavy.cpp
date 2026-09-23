#include <dspsim/dspsim.h>
#include <systemc>
#include "heavy.h"
#include "utils.h"

using namespace benchmarks;
using namespace dspsim;

int sc_main(int argc, char *argv[])
{
    size_t n_iter = argc > 1 ? std::stoul(argv[1]) : 1000000;
    size_t run_step = argc > 2 ? std::stoul(argv[2]) : 10;
    constexpr int func_iters = 500;

    auto dspsim_result = dspsim_runner<HeavyDff<double, func_iters>>(n_iter, run_step);
    auto systemc_result = systemc_runner<HeavyDffSC<double, func_iters>>(n_iter, run_step);

    return 0;
}
