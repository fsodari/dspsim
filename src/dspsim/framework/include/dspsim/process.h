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
    class SensitivityEvent;
    class Process
    {
        Context *_context;
        Model *_source;
        uint32_t _id;

        // Set while this process sits in Context::_process_eval_stack; used by FlaggedStack.
        bool _scheduled = false;

        // Initialize this process on startup.
        bool _initialize = true;

    public:
        std::function<void()> eval;

    private:
        std::string _name;

    public:
        Process(uint32_t id, std::function<void()> eval, Model *source, const std::string &name = "");

        Model *source() const { return _source; }
        uint32_t id() const { return _id; }
        const std::string &name() const { return _name; }
        bool &_scheduled_flag() { return _scheduled; }

        // If initialize is true(default), the process will be queued for evaluation at the start of simulation
        // regardless if any events have occurred.
        // Accessor for the initialization flag.
        bool &initialize() { return _initialize; }
        // Set the initialization flag. Can be chained with always() calls.
        Process *initialize(bool init)
        {
            _initialize = init;
            return this;
        }

        // Python/nanobind will need getter/setters with different names.
        bool &_get_initialize() { return initialize(); }
        Process *_set_initialize(bool init) { return initialize(init); }

        //
        // Link the process to a sensitivity event. This will ensure the process is triggered when the event occurs.
        void link_event(SensitivityEvent *event);
        // "*" can be used to say that its sensitive to changes on all inputs.
        void link_event(const std::string &event_name);
        // Used by python
        void _link_event(SensitivityEvent *event) { link_event(event); }
        void _link_event_str(const std::string &event_name) { link_event(event_name); }

        // void always(SensitivityEvent *event, Process *process = nullptr) { _always.link_process(event, process); }
        template <typename... Args>
        Process *always(Args &&...args)
        {
            // The comma operator executes print_item for each argument in sequence
            (link_event(std::forward<Args>(args)), ...);
            return this;
        }

        Process *_always_str(const std::string &event_name)
        {
            link_event(event_name);
            return this;
        }
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
    context()->register_method(&std::remove_reference<decltype(*this)>::type::method, this, std::string(#method))

//
