/*
    Verilated models: the same SystemVerilog module, verilated as a C++ model inside a dspsim module and as
    the SystemC model that Verilator writes (--sc). See models/verilated.h and hdl/Bus.sv.

    bus: 100 verilated register stages in series. A stage does almost nothing, so the cost is what an engine
         does around a model: copying port values in and out, evaluating it, committing its outputs.

    The kinds are the ways hdl/Bus.sv declares the bus of a stage, and for a struct the two ways that dspsim
    presents a struct port:

        kind             SystemVerilog       dspsim                                   SystemC (verilator --sc)
        ports            a port per field    a port per field                         a port per field
        packed           a packed struct     one port with the struct as its value    one 64-bit port
        unpacked         an unpacked struct  one port with the struct as its value    not supported by Verilator
        packed_fields    a packed struct     a PortStruct: a port per field, each a   one 64-bit port
                                             range of bits of the model
        unpacked_fields  an unpacked struct  a PortStruct: a port per field           not supported by Verilator
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
        // A struct port with the struct as its value, which is what the generator writes by default.
        suite.add("bus/n=100", "packed", params)
            .dspsim<ClockedTb<VBusPipeline<VBusValues<BusPackedValueModel, PackedBusValue>>, T>>(stages)
            .systemc<ClockedTbSC<VBusPipelineSC<VBusVectorSC>, T>>(stages);
        suite.add("bus/n=100", "unpacked", params)
            .dspsim<ClockedTb<VBusPipeline<VBusValues<BusUnpackedValueModel, UnpackedBusValue>>, T>>(stages);
        // A struct port with a port per field (struct_ports = "fields").
        suite.add("bus/n=100", "packed_fields", params)
            .dspsim<ClockedTb<VBusPipeline<VBusStruct<BusPackedModel>>, T>>(stages)
            .systemc<ClockedTbSC<VBusPipelineSC<VBusVectorSC>, T>>(stages);
        suite.add("bus/n=100", "unpacked_fields", params)
            .dspsim<ClockedTb<VBusPipeline<VBusStruct<BusUnpackedModel>>, T>>(stages);
    }
    return suite.main(argc, argv);
}
