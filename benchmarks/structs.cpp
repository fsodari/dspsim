/*
    Struct ports: pipelines of registers that pass a struct, a struct of arrays or an array of structs from
    stage to stage. See models/structs.h.

    bus:    every link is a struct with two fields and a nested struct.
    duplex: the bus, and a field that goes the other way, from every stage back to the one before it.
    arrays: every link is 16 elements with two fields each.

    The kinds of a design are not process kinds here (every process is a method) but ways to declare the
    ports of a link:

        kind        dspsim                                          SystemC
        ports       a port per field, each bound on its own         a port per field
        struct      a PortStruct, bound as a whole                  a port per field
        value       one port with the struct as its value           one port with the struct as its value
        soa         a PortStruct of port arrays                     a port per field
        soa_value   one port with a struct of arrays as its value   the same
        aos         an array of PortStructs, element by flat index  a port per field
        aos_value   an array of ports with a struct as their value  a vector of such ports
        aos_view    as aos, accessed through the views by field
        aos_index   as aos, accessed as ports[{i}].field

    A PortStruct is a group of ports: every field has a signal, and the kinds with one are the same simulation
    as "ports", reached through a struct. SystemC has no such group, so they are compared with its ports. In
    the value kinds a struct is one signal in both engines.
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
            .systemc<ClockedTbSC<BusPipelineSC<BusFieldsSC>, T>>(stages);
        suite.add("bus/n=200", "value", params)
            .dspsim<ClockedTb<BusPipeline<BusSignal>, T>>(stages)
            .systemc<ClockedTbSC<BusPipelineSC<BusValueSC>, T>>(stages);

        // The acknowledgements take as long to get back to the source as the bus takes to the sink.
        suite.add("duplex/n=200", "ports", params)
            .dspsim<ClockedTb<DuplexPipeline<DuplexFields>, T>>(stages)
            .systemc<ClockedTbSC<DuplexPipelineSC<DuplexFieldsSC>, T>>(stages);
        suite.add("duplex/n=200", "struct", params)
            .dspsim<ClockedTb<DuplexPipeline<DuplexStruct>, T>>(stages)
            .systemc<ClockedTbSC<DuplexPipelineSC<DuplexFieldsSC>, T>>(stages);
    }
    {
        constexpr int k = 16;
        constexpr int stages = 32;
        const Params params{.iters = 6'000, .warmup = 1'000};
        suite.add("arrays/k=16,n=32", "ports", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayFields<k>>, T>>(stages)
            .systemc<ClockedTbSC<ArrayPipelineSC<ArrayFieldsSC<k>>, T>>(stages);
        suite.add("arrays/k=16,n=32", "soa", params)
            .dspsim<ClockedTb<ArrayPipeline<ArraySoa<k>>, T>>(stages)
            .systemc<ClockedTbSC<ArrayPipelineSC<ArrayFieldsSC<k>>, T>>(stages);
        suite.add("arrays/k=16,n=32", "soa_value", params)
            .dspsim<ClockedTb<ArrayPipeline<ArraySoaValue<k>>, T>>(stages)
            .systemc<ClockedTbSC<ArrayPipelineSC<ArraySoaValueSC<k>>, T>>(stages);
        suite.add("arrays/k=16,n=32", "aos", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayAos<k>>, T>>(stages)
            .systemc<ClockedTbSC<ArrayPipelineSC<ArrayFieldsSC<k>>, T>>(stages);
        suite.add("arrays/k=16,n=32", "aos_value", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayAosValue<k>>, T>>(stages)
            .systemc<ClockedTbSC<ArrayPipelineSC<ArrayAosValueSC<k>>, T>>(stages);
        // Two more ways to reach the ports of an array of structs.
        suite.add("arrays/k=16,n=32", "aos_view", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayAosView<k>>, T>>(stages);
        suite.add("arrays/k=16,n=32", "aos_index", params)
            .dspsim<ClockedTb<ArrayPipeline<ArrayAosIndex<k>>, T>>(stages);
    }
    return suite.main(argc, argv);
}
