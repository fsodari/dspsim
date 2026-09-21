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

        Process(Context *context, uint32_t id, std::function<void()> eval);

        uint32_t id() const;
    };

    // Custom utility function
    template <typename MemberFunc, typename ClassType>
    auto method_to_function(MemberFunc mem_ptr, ClassType *instance)
    {
        // Returns a lambda capturing the method pointer and instance pointer
        return [instance, mem_ptr]() -> decltype(auto)
        {
            return (instance->*mem_ptr)();
        };
    }

#define DSPSIM_PROCESS(method) \
    context()->register_process(method_to_function(method, this));
    // context()->register_process(&Dff<T>::some_process, this);
}