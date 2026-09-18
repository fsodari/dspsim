#pragma once
#include <vector>
#include <queue>

namespace dspsim
{
    // A stack that maintains its elements in sorted order such that the smallest element is always on top.
    // Backed by a min-heap for O(log n) push/pop instead of a shifted, sorted vector.
    template <typename T, typename ORDER = std::greater<T>>
    class SortedStack
    {
        using HeapType = std::priority_queue<T, std::vector<T>, ORDER>;

    public:
        void push(const T &item) { _heap.push(item); }

        T pop()
        {
            T item = _heap.top();
            _heap.pop();
            return item;
        }

        const T &top() const { return _heap.top(); }

        bool empty() const { return _heap.empty(); }
        size_t size() const { return _heap.size(); }

        void clear() { _heap = HeapType(); }

    private:
        HeapType _heap;
    };
} // namespace dspsim