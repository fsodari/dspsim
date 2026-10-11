#pragma once
#include <vector>
#include <cstdint>
#include <ranges>
#include <algorithm>

namespace dspsim
{
    // Stack-like container that only allows unique elements based on their id().
    // T must be a pointer whose pointee exposes a dense, small, non-negative id() (e.g. Model*).
    // Membership is tracked by indexing a flag array with id() instead of hashing, giving true
    // O(1) push/pop with no hashing/bucket overhead.
    //
    // Intentionally unconstrained (no concept) so that T's pointee can still be an incomplete
    // type where UniqueStack<T> is merely named as a member; ptr->id() is only checked when a
    // member function that calls it is actually instantiated, same as any ordinary template.
    template <typename T>
    class UniqueStack
    {
    public:
        // STL operations expect this.
        using value_type = T;
        using const_reference = const T &;
        using iterator = typename std::vector<T>::iterator;
        using const_iterator = typename std::vector<T>::const_iterator;

        // Small default: most UniqueStacks are per-signal/per-port sensitivity lists with
        // only a handful of subscribers. Context's own shared eval/update stacks pass an
        // explicit larger capacity since those can hold every process/signal in the design.
        explicit UniqueStack(size_t initial_capacity = 8)
        {
            stack_.reserve(initial_capacity);
            in_stack_.reserve(initial_capacity);
        }
        const T &back() const
        {
            return stack_.back();
        }

        bool empty() const
        {
            return stack_.empty();
        }

        size_t size() const
        {
            return stack_.size();
        }

        void push_back(const T &element)
        {
            uint32_t idx = element->id();
            if (idx >= in_stack_.size())
            {
                in_stack_.resize(idx + 1, 0);
            }
            if (!in_stack_[idx])
            {
                in_stack_[idx] = 1;
                stack_.push_back(element);
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
            T element = stack_.back();
            stack_.pop_back();
            in_stack_[element->id()] = 0;
        }

        void clear()
        {
            stack_.clear();
            in_stack_.clear();
        }

        // Allow iterating the stack.
        auto begin() { return stack_.begin(); }
        auto end() { return stack_.end(); }
        auto begin() const { return stack_.begin(); }
        auto end() const { return stack_.end(); }

        // Find
        iterator find(const T &element)
        {
            uint32_t idx = element->id();
            if (idx < in_stack_.size() && in_stack_[idx])
            {
                return std::find(stack_.begin(), stack_.end(), element);
            }
            return stack_.end();
        }

        // Erasing operations.
        iterator erase(iterator it)
        {
            if (it != stack_.end())
            {
                in_stack_[(*it)->id()] = 0;
                return stack_.erase(it);
            }
            return stack_.end();
        }
        iterator erase(const T &element)
        {
            auto it = find(element);
            if (it != stack_.end())
            {
                in_stack_[(*it)->id()] = 0;
                return stack_.erase(it);
            }
            return stack_.end();
        }

        std::vector<T> &stack() { return stack_; }
        const std::vector<T> &stack() const { return stack_; }

    private:
        std::vector<T> stack_;
        // Indexed by element->id(); uint8_t avoids std::vector<bool>'s bit-packing overhead.
        std::vector<uint8_t> in_stack_;
    };

} // namespace dspsim