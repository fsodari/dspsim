/*
    Benchmark harness.

    A benchmark executable defines a Suite of cases. A case is one design run by one or more engines
    ("dspsim", "systemc"), each given as a testbench class, and all engines of a case must produce the same
    checksum. A testbench provides:

        Tb(args...)                 build the design
        void start()                elaborate and settle it: everything up to the first time step
        auto step(Stimulus &)       advance one iteration and return the value to add to the checksum
        uint64_t time() const       simulated time, in clock time units

    The measured loop is the same code for every engine (measure() below): start, warm up, then count
    instructions and cycles over `iters` steps. Construction, elaboration and the warm-up are not counted.

    One run of the executable measures one engine of one case and prints one record, so every measurement
    starts from a fresh process (SystemC elaborates once per process anyway). scripts/benchmarks.py runs the
    cases, repeats them, checks the checksums and reports the comparison.

    This file depends on nothing but the standard library: see dspsim_tb.h and systemc_tb.h for the engines.
*/
#pragma once
#include "counters.h"

#include <bit>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace benchmarks
{
    /// Clock period of the benchmark designs, in time units. A step usually runs one period.
    inline constexpr uint64_t clock_period = 10;
    /*
        Time between a rising clock edge and the end of a step. A testbench that changes inputs between
        steps then changes them away from the clock edge, as a testbench for clocked logic should, and the
        result does not depend on how an engine orders an input change and a clock edge at the same time.
    */
    inline constexpr uint64_t step_offset = 2;

    /*
        Deterministic stimulus (splitmix64): the same sequence in every engine, with every standard library
        and in any language, which the distributions of <random> do not give.
    */
    class Stimulus
    {
    public:
        explicit Stimulus(uint64_t seed) : state_(seed) {}

        /// The next 64 random bits.
        uint64_t next()
        {
            uint64_t z = (state_ += 0x9E3779B97F4A7C15u);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
            return z ^ (z >> 31);
        }

        /// A random value of type T: any value of an integral type, or a floating point value in [0, 100).
        template <typename T>
        T value()
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                // 53 random bits make a double in [0, 1).
                return static_cast<T>(static_cast<double>(next() >> 11) * 0x1.0p-53 * 100.0);
            }
            else
            {
                return static_cast<T>(next());
            }
        }

        /// A random integer in [0, bound).
        uint32_t below(uint32_t bound)
        {
            return static_cast<uint32_t>(((next() >> 32) * bound) >> 32);
        }

        /// True with a probability of percent / 100.
        bool chance(uint32_t percent)
        {
            return below(100) < percent;
        }

    private:
        uint64_t state_;
    };

    /*
        Checksum of the sequence of values a testbench returns from step(): a 64-bit hash that depends on
        every value, on their order and on how many there were. It also counts how often the value changed,
        because two designs that both do nothing agree on any checksum.
    */
    class Checksum
    {
    public:
        template <typename T>
        void add(const T &value)
        {
            const uint64_t bits = to_bits(value);
            changes_ += static_cast<uint64_t>(count_ != 0 && bits != last_);
            last_ = bits;
            ++count_;
            hash_ = (hash_ ^ bits) * 0x9E3779B97F4A7C15u;
            hash_ ^= hash_ >> 32;
        }

        /// The checksum of the values added so far.
        uint64_t value() const
        {
            uint64_t h = (hash_ ^ count_) * 0xBF58476D1CE4E5B9u;
            return h ^ (h >> 31);
        }
        /// Number of values that differed from the value before them.
        uint64_t changes() const { return changes_; }

    private:
        // Integers are extended to 64 bits, floating point values contribute their bit pattern.
        template <typename T>
        static uint64_t to_bits(const T &value)
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                return std::bit_cast<uint64_t>(static_cast<double>(value));
            }
            else
            {
                return static_cast<uint64_t>(value);
            }
        }

    private:
        uint64_t hash_ = 0x243F6A8885A308D3u;
        uint64_t last_ = 0;
        uint64_t count_ = 0;
        uint64_t changes_ = 0;
    };

    /// How long a case runs. The executable's --iters, --warmup and --seed options override these.
    struct Params
    {
        /// Measured iterations: calls of the testbench's step().
        uint64_t iters = 10'000;
        /// Iterations before the measurement starts. They bring the design into its steady state (a pipeline
        /// that is still filling does less work) and warm the caches. 0 means a tenth of iters.
        uint64_t warmup = 0;
        uint64_t seed = 42;
    };

    /// The record of one run.
    struct Result
    {
        Params params;
        uint64_t checksum = 0;
        /// Number of times the checksummed value changed. 0 means the design did nothing.
        uint64_t changes = 0;
        /// Simulated time covered by the measured iterations.
        uint64_t sim_time = 0;
        /// Counts over the measured iterations.
        Counts counts;
        /// Time to construct the design, and to elaborate and settle it. Reported, not compared.
        uint64_t build_ns = 0;
        uint64_t start_ns = 0;
        uint64_t peak_rss_kb = 0;
    };

    template <typename Tb>
    concept Testbench = requires(Tb tb, Stimulus &stimulus, Checksum &checksum) {
        tb.start();
        checksum.add(tb.step(stimulus));
        { tb.time() } -> std::convertible_to<uint64_t>;
    };

    /// Build a testbench as Tb(args...), start it, warm it up and measure params.iters steps.
    template <Testbench Tb, typename... Args>
    Result measure(const Params &params, const Args &...args)
    {
        Result result;
        result.params = params;

        const uint64_t build_begin = now_ns();
        auto tb = std::make_unique<Tb>(args...);
        const uint64_t start_begin = now_ns();
        tb->start();
        result.build_ns = start_begin - build_begin;
        result.start_ns = now_ns() - start_begin;

        Stimulus stimulus{params.seed};
        for (uint64_t i = 0; i < params.warmup; ++i)
        {
            tb->step(stimulus);
        }

        Checksum checksum;
        Counters counters;
        const uint64_t time_begin = tb->time();
        counters.start();
        for (uint64_t i = 0; i < params.iters; ++i)
        {
            checksum.add(tb->step(stimulus));
        }
        result.counts = counters.stop();

        result.sim_time = tb->time() - time_begin;
        result.checksum = checksum.value();
        result.changes = checksum.changes();
        result.peak_rss_kb = peak_rss_kb();

        // The process exits after one measurement, so the design is not torn down.
        (void)tb.release();
        return result;
    }

    class Case
    {
    public:
        /// Measures one engine with the given parameters.
        using Runner = std::function<Result(const Params &)>;

        struct Engine
        {
            std::string name;
            Runner run;
        };

        Case(std::string group, std::string kind, Params params, std::string description);

        /// Add an engine. Each run constructs the testbench as Tb(args...).
        template <Testbench Tb, typename... Args>
        Case &engine(const std::string &name, Args... args)
        {
            engines_.push_back({name, [args...](const Params &params)
                                { return measure<Tb>(params, args...); }});
            return *this;
        }
        template <Testbench Tb, typename... Args>
        Case &dspsim(Args... args)
        {
            return engine<Tb>("dspsim", std::move(args)...);
        }
        template <Testbench Tb, typename... Args>
        Case &systemc(Args... args)
        {
            return engine<Tb>("systemc", std::move(args)...);
        }

        /// "group/kind", unique within a suite.
        std::string name() const { return group_ + "/" + kind_; }
        const std::string &group() const { return group_; }
        const std::string &kind() const { return kind_; }
        const std::string &description() const { return description_; }
        const Params &params() const { return params_; }
        const std::vector<Engine> &engines() const { return engines_; }

    private:
        std::string group_;
        std::string kind_;
        Params params_;
        std::string description_;
        std::vector<Engine> engines_;
    };

    class Suite
    {
    public:
        explicit Suite(std::string name);

        /*
            Add a case. `group` names the design ("dff/depth=500") and `kind` how its processes are written:

                method          method process with static sensitivity      SC_METHOD, sensitive << ...
                method_dynamic  method process using next_trigger()         SC_METHOD, next_trigger()
                coro            coroutine with static sensitivity, wait()   SC_THREAD, wait()
                coro_dynamic    coroutine waiting on events, wait(event)    SC_THREAD, wait(event)

            Cases of one group are the same design and give the same checksum, so their costs can be compared.
            Add the engines to the returned case.
        */
        Case &add(std::string group, std::string kind, Params params = {}, std::string description = "");

        /// Run the command line: list the cases, or measure one engine of one case. Returns the exit status.
        int main(int argc, char *argv[]);

    private:
        std::string name_;
        std::vector<std::unique_ptr<Case>> cases_;
    };
}
