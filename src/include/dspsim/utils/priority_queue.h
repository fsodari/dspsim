#pragma once
#include <vector>
#include <queue>

namespace dspsim
{
    // Just using a C++ implementation of a priority queue.
    template <typename T, typename ORDER = std::greater<T>>
    using PriorityQueue = std::priority_queue<T, std::vector<T>, ORDER>;
} // namespace dspsim