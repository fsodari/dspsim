#pragma once
#include <cmath>
#include <cstddef>

namespace benchmarks
{
    static inline double heavy_compute(double x, size_t iters)
    {
        for (size_t i = 0; i < iters; ++i)
        {
            x = 100 * std::sin(x) * std::cos(x) + std::sqrt(std::abs(x) + 1.0);
        }
        return x;
    }
} // namespace benchmarks
