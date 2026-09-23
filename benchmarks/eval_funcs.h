#pragma once
#include <cmath>
#include <cstddef>

namespace benchmarks
{
    template <typename T>
    static inline T heavy_compute(T x, size_t iters)
    {
        for (size_t i = 0; i < iters; ++i)
        {
            x = std::sin(x) * std::cos(x) + std::sqrt(std::abs(x) + 1.0);
        }
        return x;
    }

    // template <typename T>
    // static inline T mult_random(T in, size_t iters)
    // {
    //     for (size_t i = 0; i < iters; ++i)
    //     {
    //         in *= Random::get();
    //     }
    //     return in;
    // }
} // namespace benchmarks