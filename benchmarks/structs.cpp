/*
    Struct ports: pipelines of registers that pass a struct, a struct of arrays or an array of structs from
    stage to stage. See models/structs.h.

    bus:    every link is a struct with two fields and a nested struct: four signals.
    arrays: every link is 16 elements with two fields each: 32 signals.

    The kinds of a design are not process kinds here (every process is a method) but ways to declare the
    ports of a link, and they show what a struct costs compared to the same ports without one:

        kind        dspsim                                           SystemC
        ports       a port per field, each bound on its own          a port per field
        struct      a PortStruct, bound as a whole                   one port with a struct as its value
        soa         a PortStruct of port arrays                      one port with a struct of arrays as its value
        aos         an array of PortStructs, element by flat index   a vector of ports with a struct as their value
        aos_view    the same, accessed through the views by field
        aos_index   the same, accessed as ports[{i}].field

    In dspsim a struct is a group of ports and every field has a signal, so the kinds of a design are the same
    simulation and differ in how the ports are reached. In SystemC a struct port is one signal.
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/structs.h"
#include "models/structs_sc.h"

using namespace benchmarks;

int sc_main(int argc, char *argv[])
{
    Suite suite{"structs"};
    using T = uint32_t;

    {
        // The warm-up has to fill the pipeline.
        constexpr int stages = 200;
        const Params params{.iters = 6'000, .warmup = 1'000};
        suite.add("bus/n=200", "ports", params)
            .dspsim<ClockedTb<BusPipeline<BusFields>, T>>(stages)
            .systemc<ClockedTbSC<BusPipelineSC<BusFieldsSC>, T>>(stages);
        suite.add("bus/n=200", "struct", params)
            .dspsim<ClockedTb<BusPipeline<BusStruct>, T>>(stages)
            .systemc<ClockedTbSC<BusPipelineSC<BusValueSC>, T>>(stages);
    }
    {
        constexpr int k = 16;
        constexpr int stages = 32;
        const Params params{.iters = 6'000, .warmup = 1'000};
        suite.add("arrays/k=16,n=32", "ports", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayFields<k>, k>, T>>(stages)
            .systemc<ClockedTbSC<ArrayPipelineSC<ArrayFieldsSC<k>>, T>>(stages);
        suite.add("arrays/k=16,n=32", "soa", params)
            .dspsim<ClockedTb<ArrayPipeline<ArraySoa<k>, k>, T>>(stages)
            .systemc<ClockedTbSC<ArrayPipelineSC<ArraySoaSC<k>>, T>>(stages);
        suite.add("arrays/k=16,n=32", "aos", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayAos<k>, k>, T>>(stages)
            .systemc<ClockedTbSC<ArrayPipelineSC<ArrayAosSC<k>>, T>>(stages);
        // Two more ways to reach the ports of an array of structs. They have no counterpart in SystemC.
        suite.add("arrays/k=16,n=32", "aos_view", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayAosView<k>, k>, T>>(stages);
        suite.add("arrays/k=16,n=32", "aos_index", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayAosIndex<k>, k>, T>>(stages);
    }
    return suite.main(argc, argv);
}
