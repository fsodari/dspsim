#pragma once
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <type_traits>
#include <utility>
#include <functional>
#include <memory>
#include <spdlog/spdlog.h>

namespace benchmarks
{
    constexpr int min_range = 0;
    constexpr int max_range = 100;

    namespace Random
    {
        // Random number generator engine
        inline std::mt19937 engine;

        // Helper function to seed cleanly using hardware entropy and system clock
        inline void init_engine(unsigned int seed)
        {
            engine = std::mt19937{seed};
        }

        // A helper function to generate an integer within a specific range
        template <typename T>
        inline T get(T min, T max)
        {
            if constexpr (std::is_integral_v<T>)
            {
                std::uniform_int_distribution<T> dist(min, max);
                return dist(engine);
            }
            else
            {
                std::uniform_real_distribution<T> dist(min, max);
                return dist(engine);
            }
        }
    }

    // A universal wrapper to measure function execution time
    template <typename Func, typename... Args>
    auto measure_duration(Func &&func, Args &&...args)
    {
        // Invoke the function with forwarded arguments
        // std::invoke handles free functions, lambdas, and member functions seamlessly
        auto start = std::chrono::high_resolution_clock::now();
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        return elapsed.count(); // Returns duration in seconds (double)
    }

    template <typename M, typename T>
    auto dspsim_runner(size_t n_iter, int run_step = 10, unsigned int seed = 42)
    {
        Random::init_engine(seed);
        auto ctx = dspsim::Context::create();
        // ctx->set_log_level("debug");
        dspsim::Clock clk{"clk", 10};
        dspsim::Signal<T> in_signal{"in"};
        dspsim::Signal<T> out_signal{"out"};

        M some_model{"some_model"};
        some_model.clk.bind(clk);
        some_model.in.bind(in_signal);
        some_model.out.bind(out_signal);

        ctx->elaborate();
        // Lambda to change data and call sc_run every clock.
        T checksum = 0;
        auto simulate = [&](size_t n)
        {
            for (size_t i = 0; i < n; ++i)
            {
                in_signal.write(Random::get<T>(min_range, max_range));
                ctx->run(run_step);
                checksum += out_signal.read();
            }
        };

        double result = measure_duration(simulate, n_iter);
        std::cout << "DSPSim Simulation time : " << std::fixed << std::setprecision(6) << result << " seconds, Checksum: " << checksum << ", n_iter: " << n_iter << "\n";
        return result;
    }

    template <typename M, typename T>
    auto systemc_runner(size_t n_iter, int run_step = 10, unsigned int seed = 42)
    {
        Random::init_engine(seed);
        sc_core::sc_clock clk("clk", 10, sc_core::SC_NS);
        sc_core::sc_signal<T> in_signal;
        sc_core::sc_signal<T> out_signal;

        M some_module{"simple_module"};
        some_module.clk(clk);
        some_module.in(in_signal);
        some_module.out(out_signal);

        // Lambda to change data and call sc_run every clock.
        T checksum = 0;
        auto simulate = [&](size_t n)
        {
            for (size_t i = 0; i < n; ++i)
            {
                in_signal.write(Random::get<T>(min_range, max_range));
                sc_core::sc_start(run_step, sc_core::SC_NS);
                checksum += out_signal.read();
            }
        };

        double result = measure_duration(simulate, n_iter);
        std::cout << "SystemC Simulation time: " << std::fixed << std::setprecision(6) << result << " seconds, Checksum: " << checksum << ", n_iter: " << n_iter << "\n";
        return result;
    }
} // namespace benchmarks
