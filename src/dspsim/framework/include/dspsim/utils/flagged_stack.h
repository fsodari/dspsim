#pragma once
#include <vector>
#include <algorithm>

namespace dspsim
{
    // Like UniqueStack, but membership is tracked via a bool flag stored directly on the
    // pointee (through ptr->_scheduled_flag()) instead of a side array indexed by id().
    // ptr->_scheduled_flag() is already-loaded memory (the caller just dereferenced ptr to
    // get here), so this avoids the extra, dependent load into a separate id-indexed array
    // that UniqueStack needs on every push_back/find - that indirection was measured to be
    // a real latency bottleneck in the delta-cycle hot path (see perf notes in git history).
    //
    // This only works correctly when each element is a member of at most one FlaggedStack
    // at a time, which holds for Context's per-delta-cycle process/signal scheduling: a
    // process/signal is either currently scheduled for this delta cycle or it isn't.
    template <typename T>
    class FlaggedStack
    {
    public:
        using iterator = typename std::vector<T>::iterator;
        using const_iterator = typename std::vector<T>::const_iterator;

        explicit FlaggedStack(size_t initial_capacity = 1000)
        {
            _stack.reserve(initial_capacity);
        }

        bool empty() const { return _stack.empty(); }
        size_t size() const { return _stack.size(); }
        const T &back() const { return _stack.back(); }

        void push_back(const T &element)
        {
            bool &scheduled = element->_scheduled_flag();
            if (!scheduled)
            {
                scheduled = true;
                _stack.push_back(element);
            }
        }

        void pop_back()
        {
            T element = _stack.back();
            _stack.pop_back();
            element->_scheduled_flag() = false;
        }

        void clear()
        {
            for (auto &element : _stack)
            {
                element->_scheduled_flag() = false;
            }
            _stack.clear();
        }

        // Fast rejection via the flag; falls back to a linear scan only when the element
        // is actually present (same worst case as UniqueStack::find).
        iterator find(const T &element)
        {
            if (!element->_scheduled_flag())
            {
                return _stack.end();
            }
            return std::find(_stack.begin(), _stack.end(), element);
        }

        iterator erase(iterator it)
        {
            if (it != _stack.end())
            {
                (*it)->_scheduled_flag() = false;
                return _stack.erase(it);
            }
            return _stack.end();
        }
        iterator erase(const T &element)
        {
            return erase(find(element));
        }

        auto begin() { return _stack.begin(); }
        auto end() { return _stack.end(); }
        auto begin() const { return _stack.begin(); }
        auto end() const { return _stack.end(); }

        std::vector<T> &stack() { return _stack; }

    private:
        std::vector<T> _stack;
    };

} // namespace dspsim
