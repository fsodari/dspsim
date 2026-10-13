/*
    Verilated models: the same SystemVerilog module, verilated as a C++ model inside a dspsim module and as
    the SystemC model that Verilator writes (--sc). See models/verilated.h and hdl/Bus.sv.

    bus: 100 verilated register stages in series. A stage does almost nothing, so the cost is what an engine
         does around a model: copying port values in and out, evaluating it, committing its outputs.

    The kinds are the ways hdl/Bus.sv declares the bus of a stage:

        kind        SystemVerilog         dspsim                               SystemC (verilator --sc)
        ports       a port per field      a port per field                     a port per field
        packed      a packed struct       a struct port: a port per field,     one 64-bit port
                                          each a range of bits of the model
        unpacked    an unpacked struct    a struct port: a port per field      not supported by Verilator
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/verilated.h"
#include "models/verilated_sc.h"

using namespace benchmarks;

int sc_main(int argc, char *argv[])
{
    Suite suite{"verilated"};
    using T = uint32_t;

    {
        // The warm-up has to fill the pipeline.
        constexpr int stages = 100;
        const Params params{.iters = 6'000, .warmup = 1'000};
        suite.add("bus/n=100", "ports", params)
            .dspsim<ClockedTb<VBusPipeline<VBusFields>, T>>(stages)
            .systemc<ClockedTbSC<VBusPipelineSC<VBusFieldsSC>, T>>(stages);
        suite.add("bus/n=100", "packed", params)
            .dspsim<ClockedTb<VBusPipeline<VBusStruct<BusPackedModel>>, T>>(stages)
            .systemc<ClockedTbSC<VBusPipelineSC<VBusVectorSC>, T>>(stages);
        suite.add("bus/n=100", "unpacked", params)
            .dspsim<ClockedTb<VBusPipeline<VBusStruct<BusUnpackedModel>>, T>>(stages);
    }
    return suite.main(argc, argv);
}
