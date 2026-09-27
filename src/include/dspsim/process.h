/*
    A process can be registered for evaluation in the simulation context.
*/
#pragma once
#include <functional>
#include <cstdint>
#include <string>

namespace dspsim
{
    class Context;
    class SensitivityEvent;

    class ProcessBase
    {
        Context *_context;
        uint32_t _id;
        std::string _name;

        bool _scheduled = false;
        bool _initialize = true;

    public:
        ProcessBase(const std::string &name = "");
        virtual ~ProcessBase() = default;

        uint32_t id() const { return _id; }
        const std::string &name() const { return _name; }
        bool &_scheduled_flag() { return _scheduled; }

        // If initialize is true(default), the process will be queued for evaluation at the start of simulation
        // regardless if any events have occurred.
        // Accessor for the initialization flag.
        bool &initialize() { return _initialize; }
        // Set the initialization flag. Can be chained with always() calls.
        ProcessBase *initialize(bool init)
        {
            _initialize = init;
            return this;
        }

        // Python/nanobind will need getter/setters with different names.
        bool &_get_initialize() { return initialize(); }
        ProcessBase *_set_initialize(bool init) { return initialize(init); }

        // Link the process to a sensitivity event. This will ensure the process is triggered when the event occurs.
        void schedule_static_event(SensitivityEvent *event);
        // "*" can be used to say that its sensitive to changes on all inputs.
        void schedule_static_event(const std::string &event_name);

        // Used by python
        void _schedule_static_event(SensitivityEvent *event) { schedule_static_event(event); }
        void _schedule_static_event_str(const std::string &event_name) { schedule_static_event(event_name); }

        // void always(SensitivityEvent *event, Process *process = nullptr) { _always.link_process(event, process); }
        template <typename... Args>
        ProcessBase *always(Args &&...args)
        {
            // The comma operator executes print_item for each argument in sequence
            (schedule_static_event(std::forward<Args>(args)), ...);
            return this;
        }

        ProcessBase *_always_str(const std::string &event_name)
        {
            schedule_static_event(event_name);
            return this;
        }

        // Processes will call their bound eval() func. Coroutines will be resumed with their handle.
        virtual void resume() = 0;
    };

    class Process : public ProcessBase
    {
        std::function<void()> _eval;

    public:
        Process(std::function<void()> eval, const std::string &name = "");

        void resume() override { _eval(); }
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
    context()->register_method(&std::remove_reference<decltype(*this)>::type::method, this, this->hier_name() + "." + std::string(#method))

//
