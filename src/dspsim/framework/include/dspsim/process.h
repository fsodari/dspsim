/*
    A process can be registered for evaluation in the simulation context.
*/
#pragma once
#include <concepts>
#include <functional>
#include <cstdint>

namespace dspsim
{
    class Context;

    class Process
    {
        Context *_context;
        uint32_t _id;

    public:
        std::function<void()> eval;

        uint32_t id() const;
    };
}