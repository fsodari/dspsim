/*
    A process can be registered for evaluation in the simulation context.
*/
#pragma once
#include <dspsim/model.h>
#include <functional>
#include <cstdint>
#include <string>

namespace dspsim
{
    class Context;

    class Process
    {
        Context *_context;
        Model *_source;
        uint32_t _id;

    public:
        std::function<void()> eval;

    private:
        std::string _name;

    public:
        Process(Context *context, uint32_t id, std::function<void()> eval, Model *source, const std::string &name = "");

        Model *source() const;
        uint32_t id() const;
        const std::string &name() const;
    };

    // Custom utility function. Wraps a method and this ptr in a lambda.
    template <typename MemberFunc, typename ClassType>
    auto method_to_function(MemberFunc mem_ptr, ClassType *instance)
    {
        // Returns a lambda capturing the method pointer and instance pointer
        return [instance, mem_ptr]() -> decltype(auto)
        {
            return (instance->*mem_ptr)();
        };
    }
}

// Convenience macro for registering a method as a process
#define DSPSIM_METHOD(method) \
    context()->register_method(method, this, std::string(#method));

//
