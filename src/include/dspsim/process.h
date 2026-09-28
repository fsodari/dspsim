/*
    A process can be registered for evaluation in the simulation context.
*/
#pragma once
// #include <dspsim/sensitivity_list.h>
#include <dspsim/coro/coro.h>
#include <functional>
#include <cstdint>
#include <string>

namespace dspsim
{
    class Context;
    class SensitivityEvent;
    class Module;

    class ProcessBase
    {
        Context *_context;
        uint32_t _id;
        std::string _name;
        Module *_parent_module;

        bool _scheduled = false;
        bool _initialize = true;

        // Static sensitivity will be disabled until a dynamic event occurs.
        bool _static_sensitivity_disabled = false;

    public:
        ProcessBase(const std::string &name = "");
        virtual ~ProcessBase() = default;

        Context *context() const { return _context; }
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

        //
        bool static_sensitivity_disabled() { return _static_sensitivity_disabled; }
        // Return to being sensitive to static events.
        void reset_static_sensitivity() { _static_sensitivity_disabled = false; }

        void schedule_static_event(SensitivityEvent *event);
        void schedule_static_event(const std::string &event_name);

        void schedule_dynamic_event(SensitivityEvent *event);
        void schedule_dynamic_event(const std::string &event_name);

        // void always(SensitivityEvent *event, Process *process = nullptr) { _always.link_process(event, process); }
        template <typename... Args>
        ProcessBase *always(Args &&...args)
        {
            // The comma operator executes print_item for each argument in sequence
            (this->schedule_static_event(std::forward<Args>(args)), ...);
            return this;
        }

        ProcessBase *_always_str(const std::string &event_name)
        {
            this->schedule_static_event(event_name);
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

    /*
        Coroutine-based process class.
    */
    class CoroProcess : public ProcessBase
    {
        Task _task;
        // std::coroutine_handle<> _handle;

    public:
        // CoroProcess(std::coroutine_handle<> handle, const std::string &name = "");
        CoroProcess(Task task, const std::string &name = "");

        void resume() override;
    };
}

// Convenience macro for registering a method as a process
#define DSPSIM_METHOD(method) \
    context()->register_method(&std::remove_reference<decltype(*this)>::type::method, this, this->hier_name() + "." + std::string(#method))

// Convenience macro for registering a coroutine task as a process
#define DSPSIM_CORO(task) \
    context()->register_coro_task(task(), this->hier_name() + "." + std::string(#task))
//     context()->_processes.emplace_back(std::make_unique<CoroProcess>(some_task(), "some_task"));
//     context()->register_coro_task(&std::remove_reference<decltype(*this)>::type::task, this, this->hier_name() + "." + std::string(#task))
//
