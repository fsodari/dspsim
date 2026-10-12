/*
    A pipeline with a valid/ready handshake: register-transfer level code with clocked and combinational
    processes, submodules, arrays of signals and back-pressure. See models/stream.h.
*/
#include "harness/dspsim_tb.h"
#include "harness/systemc_tb.h"
#include "models/stream.h"
#include "models/stream_sc.h"

#include <string>

using namespace benchmarks;

namespace
{
    using T = uint32_t;
    using PipelineMethod = Pipeline<SourceMethod<T>, SliceMethod<T>, SinkMethod<T>, T>;
    using PipelineCoro = Pipeline<SourceCoro<T>, SliceCoro<T>, SinkCoro<T>, T>;
    using PipelineMethodSC = PipelineSC<SourceMethodSC<T>, SliceMethodSC<T>, SinkMethodSC<T>, T>;
    using PipelineThreadSC = PipelineSC<SourceThreadSC<T>, SliceThreadSC<T>, SinkThreadSC<T>, T>;

    void add_pipeline(Suite &suite, int stages, Params params)
    {
        const std::string group = "pipeline/n=" + std::to_string(stages);
        suite.add(group, "method", params)
            .dspsim<ClockedTb<PipelineMethod, T>>(stages)
            .systemc<ClockedTbSC<PipelineMethodSC, T>>(stages);
        suite.add(group, "coro", params)
            .dspsim<ClockedTb<PipelineCoro, T>>(stages)
            .systemc<ClockedTbSC<PipelineThreadSC, T>>(stages);
    }
}

int sc_main(int argc, char *argv[])
{
    Suite suite{"stream"};
    // The warm-up has to fill the pipeline.
    add_pipeline(suite, 8, {.iters = 150'000});
    add_pipeline(suite, 256, {.iters = 6'000, .warmup = 2'000});
    return suite.main(argc, argv);
}
