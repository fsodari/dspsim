#pragma once
#include <vector>
#include <algorithm>

namespace dspsim
{
    // Like UniqueStack, but membership is tracked via a bool flag stored directly on the
    // pointee (through ptr->scheduled_flag()) instead of a side array indexed by id().
    // ptr->scheduled_flag() is already-loaded memory (the caller just dereferenced ptr to
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
            stack_.reserve(initial_capacity);
        }

        bool empty() const { return stack_.empty(); }
        size_t size() const { return stack_.size(); }
        const T &back() const { return stack_.back(); }

        void push_back(const T &element)
        {
            bool &scheduled = element->scheduled_flag();
            if (!scheduled)
            {
                scheduled = true;
                stack_.push_back(element);
            }
        }

        void pop_back()
        {
            T element = stack_.back();
            stack_.pop_back();
            element->scheduled_flag() = false;
        }

        void clear()
        {
            for (auto &element : stack_)
            {
                element->scheduled_flag() = false;
            }
            stack_.clear();
        }

        // Fast rejection via the flag; falls back to a linear scan only when the element
        // is actually present (same worst case as UniqueStack::find). The scan starts at the back:
        // the usual lookup is for an element that was just pushed (e.g. ProcessBase::initialize(false)).
        iterator find(const T &element)
        {
            if (!element->scheduled_flag())
            {
                return stack_.end();
            }
            auto it = std::find(stack_.rbegin(), stack_.rend(), element);
            return it == stack_.rend() ? stack_.end() : std::next(it).base();
        }

        iterator erase(iterator it)
        {
            if (it != stack_.end())
            {
                (*it)->scheduled_flag() = false;
                return stack_.erase(it);
            }
            return stack_.end();
        }
        iterator erase(const T &element)
        {
            return erase(find(element));
        }

        auto begin() { return stack_.begin(); }
        auto end() { return stack_.end(); }
        auto begin() const { return stack_.begin(); }
        auto end() const { return stack_.end(); }

        std::vector<T> &stack() { return stack_; }

    private:
        std::vector<T> stack_;
    };

} // namespace dspsim
