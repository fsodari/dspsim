#pragma once
#include <dspsim/utils/inlining.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>


namespace dspsim
{
    /*
        A stack whose push_back() is a few instructions that the compiler always inlines, for the scheduling
        lists of the run cycle. It is FlaggedStack without the flag: it keeps its own end and capacity
        pointers into a std::vector that only provides the storage, and the path that makes the storage
        larger is a function of its own. std::vector::push_back() carries that path inline, which makes
        every function that pushes large and makes it keep registers on the stack.
    */
    template <typename T>
    class PlainStack
    {
    public:
        using iterator = T *;
        using const_iterator = const T *;

        explicit PlainStack(size_t initial_capacity = 1000)
            : slots_(std::max<size_t>(initial_capacity, 1)),
              end_(slots_.data()),
              cap_(slots_.data() + slots_.size())
        {
        }

        // The end and capacity pointers refer to this stack's own storage: moving takes the storage over.
        PlainStack(const PlainStack &) = delete;
        PlainStack &operator=(const PlainStack &) = delete;
        PlainStack(PlainStack &&other) noexcept
            : slots_(std::move(other.slots_)),
              end_(other.end_),
              cap_(other.cap_)
        {
            other.end_ = other.slots_.data();
            other.cap_ = other.slots_.data() + other.slots_.size();
        }

        bool empty() const { return end_ == slots_.data(); }
        size_t size() const { return static_cast<size_t>(end_ - slots_.data()); }

        void push_back(const T &element)
        {
            if (end_ == cap_) [[unlikely]]
            {
                grow();
            }
            *end_++ = element;
        }

        void clear()
        {
            end_ = slots_.data();
        }

        iterator begin() { return slots_.data(); }
        iterator end() { return end_; }
        const_iterator begin() const { return slots_.data(); }
        const_iterator end() const { return end_; }

    private:
        // Double the storage. Called when the stack is full. Never inlined, see the class comment.
        DSPSIM_NOINLINE void grow()
        {
            const size_t count = size();
            slots_.resize(std::max<size_t>(slots_.size() * 2, 16));
            end_ = slots_.data() + count;
            cap_ = slots_.data() + slots_.size();
        }

    private:
        // Storage. The stack is [slots_.data(), end_).
        std::vector<T> slots_;
        T *end_;
        T *cap_;
    };

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
    //
    // The stack keeps its own end and capacity pointers into a std::vector that only provides the storage.
    // std::vector::push_back() takes its argument by reference, so unless the compiler inlines the vector's
    // growth path, the caller has to spill the pointer it pushes to the stack on every call. push_back() here
    // never takes the address of its argument, whatever the compiler decides to inline.
    template <typename T>
    class FlaggedStack
    {
    public:
        using iterator = T *;
        using const_iterator = const T *;

        explicit FlaggedStack(size_t initial_capacity = 1000)
            : slots_(std::max<size_t>(initial_capacity, 1)),
              end_(slots_.data()),
              cap_(slots_.data() + slots_.size())
        {
        }

        // The end and capacity pointers refer to this stack's own storage: moving takes the storage over.
        FlaggedStack(const FlaggedStack &) = delete;
        FlaggedStack &operator=(const FlaggedStack &) = delete;
        FlaggedStack(FlaggedStack &&other) noexcept
            : slots_(std::move(other.slots_)),
              end_(other.end_),
              cap_(other.cap_)
        {
            other.end_ = other.slots_.data();
            other.cap_ = other.slots_.data() + other.slots_.size();
        }

        bool empty() const { return end_ == slots_.data(); }
        size_t size() const { return static_cast<size_t>(end_ - slots_.data()); }
        const T &back() const { return end_[-1]; }

        void push_back(const T &element)
        {
            bool &scheduled = element->scheduled_flag();
            if (!scheduled)
            {
                scheduled = true;
                if (end_ == cap_) [[unlikely]]
                {
                    grow();
                }
                *end_++ = element;
            }
        }

        void pop_back()
        {
            T element = *--end_;
            element->scheduled_flag() = false;
        }

        void clear()
        {
            for (T &element : *this)
            {
                element->scheduled_flag() = false;
            }
            end_ = slots_.data();
        }

        // Empty the stack without resetting the flags of its elements. For when the elements may no longer exist.
        void drop()
        {
            end_ = slots_.data();
        }

        // Make room for at least `capacity` elements.
        void reserve(size_t capacity)
        {
            if (capacity > slots_.size())
            {
                resize_storage(capacity);
            }
        }

        // Fast rejection via the flag; falls back to a linear scan only when the element
        // is actually present (same worst case as UniqueStack::find). The scan starts at the back:
        // the usual lookup is for an element that was just pushed (e.g. ProcessBase::initialize(false)).
        iterator find(const T &element)
        {
            if (!element->scheduled_flag())
            {
                return end();
            }
            for (iterator it = end(); it != begin();)
            {
                if (*--it == element)
                {
                    return it;
                }
            }
            return end();
        }

        iterator erase(iterator it)
        {
            if (it != end())
            {
                (*it)->scheduled_flag() = false;
                std::move(it + 1, end(), it);
                --end_;
                return it;
            }
            return end();
        }
        iterator erase(const T &element)
        {
            return erase(find(element));
        }

        iterator begin() { return slots_.data(); }
        iterator end() { return end_; }
        const_iterator begin() const { return slots_.data(); }
        const_iterator end() const { return end_; }

    private:
        // Double the storage. Called when the stack is full.
        // Never inlined: with the vector's reallocation in its body, every caller of push_back() carries
        // that code, and the compiler keeps registers on the stack around it or stops inlining push_back().
        DSPSIM_NOINLINE void grow()
        {
            resize_storage(std::max<size_t>(slots_.size() * 2, 16));
        }

        void resize_storage(size_t capacity)
        {
            const size_t count = size();
            slots_.resize(capacity);
            end_ = slots_.data() + count;
            cap_ = slots_.data() + slots_.size();
        }

    private:
        // Storage. The stack is [slots_.data(), end_).
        std::vector<T> slots_;
        T *end_;
        T *cap_;
    };

} // namespace dspsim

