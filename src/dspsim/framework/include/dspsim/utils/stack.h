#pragma once
#include <stack>
#include <deque>

namespace dspsim
{
    // Inherit from std::stack to expose the protected member 'c'
    // Iterable stack that exposes the underlying container's iterators
    template <typename T, typename Container = std::deque<T>>
    class Stack : public std::stack<T, Container>
    {
    public:
        // auto begin() { return this->c.begin(); }
        // auto end() { return this->c.end(); }
        auto begin() const { return this->c.begin(); }
        auto end() const { return this->c.end(); }
    };
} // namespace dspsim