#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <queue>
#include <vector>

namespace dspsim
{
    /// std::priority_queue (the smallest element on top by default), with the removal of arbitrary elements.
    template <typename T, typename ORDER = std::greater<T>>
    class PriorityQueue : public std::priority_queue<T, std::vector<T>, ORDER>
    {
    public:
        using std::priority_queue<T, std::vector<T>, ORDER>::priority_queue;

        /// Remove every element for which the predicate is true. Returns the number of elements removed.
        /// Linear in the size of the queue, and a queue that loses no element is left as it is.
        template <typename Predicate>
        std::size_t erase_if(Predicate predicate)
        {
            const std::size_t removed = std::erase_if(this->c, predicate);
            if (removed != 0)
            {
                std::make_heap(this->c.begin(), this->c.end(), this->comp);
            }
            return removed;
        }
    };
} // namespace dspsim
