/*
    Counters for a region of code, read for the calling thread.

    On Linux these are hardware counters from perf_event_open(): instructions and CPU cycles in user mode,
    with L1 data cache misses and branch misses when the CPU offers them. Elsewhere only the times are
    available, plus a cycle count on Windows (QueryThreadCycleTime).

    This file depends on nothing but the standard library, so it can be used from any testbench.
    Setting the environment variable BENCH_COUNTERS=time turns the hardware counters off.
*/
#pragma once
#include <cstdint>
#include <memory>

namespace benchmarks
{
    /// What was counted between Counters::start() and Counters::stop(). A count that is not available is 0.
    struct Counts
    {
        /// Where the counts come from: "perf_event", "thread_cycle_time" or "time".
        const char *source = "time";
        /// Instructions retired in user mode.
        uint64_t instructions = 0;
        /// CPU cycles. User mode only with "perf_event"; with "thread_cycle_time" kernel time is included.
        uint64_t cycles = 0;
        uint64_t l1d_misses = 0;
        uint64_t branch_misses = 0;
        /// True if the kernel had to share the hardware counters, in which case the counts are estimates.
        bool multiplexed = false;
        /// Elapsed time, and CPU time of the calling thread.
        uint64_t wall_ns = 0;
        uint64_t cpu_ns = 0;
        /// The part of cpu_ns spent in the kernel (0 where it is not available). The hardware counters only
        /// count user mode, so a design that makes the engine call the kernel a lot shows less than it costs.
        uint64_t kernel_ns = 0;
    };

    class Counters
    {
    public:
        Counters();
        ~Counters();
        Counters(const Counters &) = delete;
        Counters &operator=(const Counters &) = delete;

        /// Start counting from zero.
        void start();
        /// Stop counting and return the counts since start().
        Counts stop();

    private:
        struct State;
        std::unique_ptr<State> state_;
    };

    /// Monotonic time in nanoseconds, for timing outside of a counted region.
    uint64_t now_ns();
    /// Peak resident memory of the process in kilobytes, or 0 if it is not available.
    uint64_t peak_rss_kb();
}
