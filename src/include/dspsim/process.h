/*
    A process can be registered for evaluation in the simulation context.
*/
#pragma once
// #include <dspsim/sensitivity_list.h>
#include <dspsim/coro.h>
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
    public:
        ProcessBase(const std::string &name = "");
        virtual ~ProcessBase() = default;

        Context *context() const { return context_; }
        uint32_t id() const { return id_; }
        const std::string &name() const { return name_; }
        bool &scheduled_flag() { return scheduled_; }

        // If initialize is true(default), the process will be queued for evaluation at the start of simulation
        // regardless if any events have occurred.
        // Accessor for the initialization flag.
        bool &initialize() { return initialize_; }
        // Set the initialization flag. Can be chained with always() calls.
        ProcessBase *initialize(bool init)
        {
            initialize_ = init;
            return this;
        }

        // Python/nanobind will need getter/setters with different names.
        bool &_get_initialize() { return initialize(); }
        ProcessBase *_set_initialize(bool init) { return initialize(init); }

        //
        bool static_sensitivity_disabled() { return static_sensitivity_disabled_; }
        // Return to being sensitive to static events.
        void reset_static_sensitivity();

        void schedule_static_event(SensitivityEvent &event);
        void schedule_static_event(const std::string &event_name);

        void schedule_dynamic_event(SensitivityEvent &event);
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

        // Indicates if the coroutine or process has completed.
        virtual bool done() const = 0;

    private:
        Context *context_;
        uint32_t id_;
        std::string name_;
        Module *parent_module_;

        bool scheduled_ = false;
        bool initialize_ = true;

        // Static sensitivity will be disabled until a dynamic event occurs.
        bool static_sensitivity_disabled_ = false;
    };

    class Process : public ProcessBase
    {

    public:
        Process(std::function<void()> eval, const std::string &name = "");

        void resume() override
        {
            eval_();
            done_ = true;
        }
        bool done() const override { return done_; }

    private:
        std::function<void()> eval_;
        bool done_ = false;
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

    public:
        CoroProcess(Task task, const std::string &name = "");

        bool done() const override;
        void resume() override;

    private:
        Task task_;
    };
}

// Convenience macro for registering a method as a process
#define DSPSIM_METHOD(method) \
    context()->register_method(&std::remove_reference<decltype(*this)>::type::method, this, this->hier_name() + "." + std::string(#method))

// Convenience macro for registering a coroutine task as a process
#define DSPSIM_CORO(task) \
    context()->register_coro_task(task(), this->hier_name() + "." + std::string(#task))
