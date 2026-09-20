#pragma once
#include <vector>
#include <cstdint>
#include <concepts>
#include <ranges>
#include <algorithm>

namespace dspsim
{
    template <typename T>
    concept UniqueStackType = requires(T ptr) {
        ptr->id();
    };

    // Stack-like container that only allows unique elements based on their id().
    // T must be a pointer whose pointee exposes a dense, small, non-negative id() (e.g. Model*).
    // Membership is tracked by indexing a flag array with id() instead of hashing, giving true
    // O(1) push/pop with no hashing/bucket overhead.
    template <UniqueStackType T>
    class UniqueStack
    {
    public:
        // STL operations expect this.
        using value_type = T;
        using const_reference = const T &;

        const T &back() const
        {
            return _stack.back();
        }

        bool empty() const
        {
            return _stack.empty();
        }

        size_t size() const
        {
            return _stack.size();
        }

        void push_back(const T &element)
        {
            uint32_t idx = element->id();
            if (idx >= _in_stack.size())
            {
                _in_stack.resize(idx + 1, 0);
            }
            if (!_in_stack[idx])
            {
                _in_stack[idx] = 1;
                _stack.push_back(element);
            }
        }

        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, T>
        void append_range(R &&rg)
        {
            std::ranges::copy(rg, std::back_inserter(*this));
        }

        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, T>
        void push_range(R &&rg)
        {
            append_range(std::forward<R>(rg));
        }

        void pop_back()
        {
            T element = _stack.back();
            _stack.pop_back();
            _in_stack[element->id()] = 0;
        }

        void clear()
        {
            _stack.clear();
            _in_stack.clear();
        }

        // Allow iterating the stack.
        auto begin() { return _stack.begin(); }
        auto end() { return _stack.end(); }
        auto begin() const { return _stack.begin(); }
        auto end() const { return _stack.end(); }

    private:
        std::vector<T> _stack;
        // Indexed by element->id(); uint8_t avoids std::vector<bool>'s bit-packing overhead.
        std::vector<uint8_t> _in_stack;
    };

} // namespace dspsim