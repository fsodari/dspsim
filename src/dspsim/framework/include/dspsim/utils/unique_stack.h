#pragma once
#include <vector>
#include <cstdint>

namespace dspsim
{
    // Unique stack implementation. Ensures that each element is only present once in the stack.
    // T must be a pointer whose pointee exposes a dense, small, non-negative id() (e.g. Model*).
    // Membership is tracked by indexing a flag array with id() instead of hashing, giving true
    // O(1) push/pop with no hashing/bucket overhead.
    template <typename T>
    class UniqueStack
    {
    public:
        void push(T element)
        {
            size_t idx = static_cast<size_t>(element->id());
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

        T pop()
        {
            T element = _stack.back();
            _stack.pop_back();
            _in_stack[static_cast<size_t>(element->id())] = 0;
            return element;
        }

        T &top()
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

        void clear()
        {
            _stack.clear();
            _in_stack.clear();
        }

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