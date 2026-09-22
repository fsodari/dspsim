// Benchmarks Context::eval()'s TMC dispatch path (tmc::post_bulk_waitable) against the inline
// path, using a large number of independent, computationally heavy processes per delta round.
// sosfilt.cpp intentionally stays below Context::parallel_eval_threshold() (tens of trivial
// processes/round); this benchmark is designed to exceed it with meaningfully expensive work.
#include <dspsim/dspsim.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace dspsim;

namespace
{
    // Data-dependent chain (each iteration depends on the previous), so the compiler can't
    // vectorize/hoist/eliminate it - it's a stand-in for a real per-cycle DSP workload.
    double heavy_compute(double x, size_t iters)
    {
        for (size_t i = 0; i < iters; ++i)
        {
            x = std::sin(x) * std::cos(x) + std::sqrt(std::abs(x) + 1.0);
        }
        return x;
    }

    class HeavyWorker : public Module
    {
    public:
        Input<uint8_t> clk{"clk"};
        Signal<double> state{"state"};
        size_t work_iters;

        HeavyWorker(ModuleName name, size_t work_iters_)
            : Module(name), work_iters(work_iters_)
        {
            state.init(1.0);
            DSPSIM_METHOD(&HeavyWorker::calc);
            sensitive << clk.pos();
            dont_initialize();
        }

        void calc()
        {
            if (clk.posedge())
            {
                state.write(heavy_compute(state.read(), work_iters));
            }
        }
    };

    struct BenchResult
    {
        double elapsed_seconds;
        // Sum of every worker's final state; keeps the optimizer from discarding the work and
        // gives a cheap way to sanity-check inline vs. parallel runs produce the same result.
        double checksum;
    };

    BenchResult run_once(size_t num_workers, size_t work_iters, size_t num_cycles, size_t parallel_threshold)
    {
        auto ctx = Context::create();
        ctx->set_parallel_eval_threshold(parallel_threshold);

        Clock clk{"clk", 10};

        std::vector<std::unique_ptr<HeavyWorker>> workers;
        workers.reserve(num_workers);
        for (size_t i = 0; i < num_workers; ++i)
        {
            // Pass a temporary ModuleName here (not a named local): ModuleName's destructor
            // finalizes the enclosing Module's construction, so it must not outlive this statement.
            workers.push_back(std::make_unique<HeavyWorker>("worker" + std::to_string(i), work_iters));
            workers.back()->clk.bind(clk);
        }

        ctx->elaborate();

        auto start_time = std::chrono::high_resolution_clock::now();
        // Two clk half-periods per full cycle, plus a little slack.
        ctx->run(static_cast<uint64_t>(clk.period()) * 2 * (num_cycles + 1));
        auto end_time = std::chrono::high_resolution_clock::now();

        double checksum = 0.0;
        for (auto &w : workers)
        {
            checksum += w->state.read();
        }

        return {std::chrono::duration<double>(end_time - start_time).count(), checksum};
    }
}

int main(int argc, char *argv[])
{
    size_t num_workers = argc > 1 ? std::stoul(argv[1]) : 256;
    size_t work_iters = argc > 2 ? std::stoul(argv[2]) : 2000;
    size_t num_cycles = argc > 3 ? std::stoul(argv[3]) : 500;

    std::cout << "workers=" << num_workers << " work_iters/worker/cycle=" << work_iters
              << " cycles=" << num_cycles << std::endl;

    // Threshold above num_workers: every round's batch size is below it, so eval() always takes
    // the inline path.
    auto inline_result = run_once(num_workers, work_iters, num_cycles, num_workers + 1);
    std::cout << "Inline eval:   " << inline_result.elapsed_seconds << "s (checksum "
              << inline_result.checksum << ")" << std::endl;

    // Threshold of 0: every non-empty round dispatches via tmc::post_bulk_waitable.
    auto parallel_result = run_once(num_workers, work_iters, num_cycles, 0);
    std::cout << "Parallel eval: " << parallel_result.elapsed_seconds << "s (checksum "
              << parallel_result.checksum << ")" << std::endl;

    std::cout << "Speedup: " << (inline_result.elapsed_seconds / parallel_result.elapsed_seconds)
              << "x" << std::endl;

    return 0;
}
