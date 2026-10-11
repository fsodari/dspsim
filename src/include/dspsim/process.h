/*
    A process can be registered for evaluation in the simulation context.
*/
#pragma once
#include <coroutine>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dspsim
{
    class Context;
    class SensitivityEvent;
    class Module;
    class WaitBase;
    template <typename T>
    class FlaggedStack;

    namespace detail
    {
        struct EngineAccess;
    }
    namespace testing
    {
        struct EngineInspector;
    }

    class ProcessBase
    {
        // Only the scheduler resumes a process and tells it what woke it.
        friend class Context;
        friend class SensitivityEvent;
        // Module::next_trigger() schedules dynamic events for the running process.
        friend class Module;
        // Leaf awaitables schedule the process's waits and record its resume point.
        friend class WaitBase;
        // The scheduling stacks keep their "is scheduled" flag in the process.
        template <typename T>
        friend class FlaggedStack;
        friend struct detail::EngineAccess;
        friend struct testing::EngineInspector;

    public:
        virtual ~ProcessBase();

        // The accessors defined in this class are read by the scheduler for every evaluation. They are inline
        // because a call into another translation unit of a shared library (the Python extension) is not
        // inlined by link-time optimization. Everything else is defined in process.cpp.
        Context *context() const { return context_; }
        uint32_t id() const { return id_; }
        const std::string &name() const;
        // True for a coroutine process: it waits with wait(...) instead of next_trigger(...), and it completes.
        bool is_coroutine() const { return coroutine_; }

        /*
            A process is scheduled for evaluation as soon as it is registered, so by default it is evaluated
            in the next delta cycle regardless of any events: at the start of the simulation, or when the
            simulation continues if it was registered later.
        */
        // True if the process is evaluated once after registration without waiting for an event.
        bool initialize() const;
        /*
            initialize(false) takes the process off the evaluation stack, so it is first evaluated when one of
            its events occurs. initialize(true) schedules it again. Can be chained with always() calls.
            Throws std::logic_error for initialize(true) on a coroutine process that has completed.
        */
        ProcessBase *initialize(bool init);

        /*
            Add events to the static sensitivity list: the process is evaluated whenever one of them occurs.
            Accepts events, signals, ports, signal/port arrays (every element), and "*" for a change of any
            input of the parent module.
        */
        template <typename... Args>
        ProcessBase *always(Args &&...args)
        {
            (this->schedule_static_item(std::forward<Args>(args)), ...);
            return this;
        }

        // Indicates if the coroutine or process has completed.
        virtual bool done() const = 0;

    protected:
        /// Processes belong to the context that registers them, not the global active context.
        /// A coroutine process (`coroutine` = true) runs once to completion instead of being evaluated repeatedly.
        ProcessBase(Context *context, const std::string &name = "", bool coroutine = false);

        /*
            Stop waiting on everything: unsubscribes from the static and dynamic events and makes pending time
            waits stale. Called when the process completes or is removed, so that nothing schedules it again.
        */
        void unschedule();

        // The suspended coroutine to resume next: the innermost one awaiting a leaf awaitable.
        std::coroutine_handle<> resume_point() const { return resume_point_; }
        void set_resume_point(std::coroutine_handle<> handle) { resume_point_ = handle; }

    private:
        /*
            Scheduler interface. Processes will call their bound eval() func. Coroutines will be resumed with
            their handle. A completed coroutine is never resumed: it unschedules itself when it completes.
        */
        virtual void resume() = 0;
        // Called by the scheduler before each resume().
        void mark_woken() { ++wake_count_; }
        // Number of times the scheduler has resumed this process. Pending time waits are tagged with it.
        uint64_t wake_count() const { return wake_count_; }
        // Set while the process sits in Context::process_eval_stack_; used by FlaggedStack.
        bool &scheduled_flag() { return scheduled_; }

        // True while the process waits on a dynamic event or a time wait.
        bool static_sensitivity_disabled() const { return static_sensitivity_disabled_; }

        void schedule_static_event(SensitivityEvent &event);
        // "*" is sensitive to a change of any input of the parent module.
        void schedule_static_event(const std::string &event_name);

        // Signal/port arrays: be sensitive to every element.
        template <typename Arr>
            requires requires(Arr &a) { a.size(); a.flat(0); }
        void schedule_static_item(Arr &array)
        {
            for (std::size_t i = 0; i < array.size(); ++i)
            {
                this->schedule_static_event(array.flat(i));
            }
        }
        template <typename Arg>
            requires(!requires(Arg &a) { a.size(); a.flat(0); })
        void schedule_static_item(Arg &&arg)
        {
            this->schedule_static_event(std::forward<Arg>(arg));
        }

        /*
            Wait on a dynamic event. Static sensitivity is disabled until a dynamic event triggers the process.
            Scheduling several dynamic events waits on any of them: the first to trigger unsubscribes the process
            from the rest.
        */
        void schedule_dynamic_event(SensitivityEvent &event);
        // "*" waits on a change of any input of the parent module.
        void schedule_dynamic_event(const std::string &event_name);

        /*
            Coroutine time waits. Unlike next_trigger(time), which schedules an extra wakeup, a time wait is the
            only thing the suspended coroutine waits for: static sensitivity is disabled until the time elapses,
            and the wakeup is dropped if anything else (a dynamic event) resumes the process first.
        */
        void schedule_time_wait(uint64_t time_delta);

        /*
            Called by a SensitivityEvent that triggered this process's dynamic sensitivity.
            Unsubscribes from the other dynamic events and restores static sensitivity.
        */
        void dynamic_event_triggered(SensitivityEvent &event);
        // Called by the scheduler when a time wait fires: the process stops waiting on everything else.
        void time_wait_triggered();

        // Return to being sensitive to static events.
        void reset_static_sensitivity();
        // Unsubscribe from all static events.
        void cancel_static_events();
        // Unsubscribe from all dynamic events without restoring static sensitivity.
        void cancel_dynamic_events();

    private:
        // Fields read on every evaluation come first so that they share a cache line with the vtable pointer.
        Context *context_;
        uint32_t id_;
        bool scheduled_ = false;
        bool initialize_ = true;
        // Static sensitivity will be disabled until a dynamic event occurs.
        bool static_sensitivity_disabled_ = false;
        bool coroutine_;
        uint64_t wake_count_ = 0;
        std::coroutine_handle<> resume_point_{};

        // Elaboration-time data.
        std::string name_;
        Module *parent_module_;
        // Static events this process is subscribed to. Cleared when the process completes.
        std::vector<SensitivityEvent *> static_events_;
        // Dynamic events this process is subscribed to. Cleared when one of them triggers.
        std::vector<SensitivityEvent *> dynamic_events_;
    };

    class Process : public ProcessBase
    {

    public:
        Process(Context *context, std::function<void()> eval, const std::string &name = "");

        bool done() const override;

    private:
        void resume() override;

    private:
        std::function<void()> eval_;
        bool done_ = false;
    };

    /*
        Process that evaluates a member function of a model. The member function is bound at compile time
        (see Context::register_method<&Model::method>(instance)) and called through a plain function pointer,
        with no std::function or heap-allocated functor: this is the hot path of method-based designs.
    */
    class MethodProcess : public ProcessBase
    {
    public:
        using Trampoline = void (*)(void *instance);

        MethodProcess(Context *context, void *instance, Trampoline call, const std::string &name = "");

        bool done() const override;

    private:
        void resume() override;

    private:
        void *instance_;
        Trampoline call_;
        bool done_ = false;
    };

    /*
        Coroutine-based process class. Owns the root coroutine frame of a Task and resumes the process's
        resume point: the root task, or the innermost nested task awaiting a leaf awaitable.
        When the root task completes, the process unschedules itself, so the scheduler never resumes it again.
    */
    class CoroProcess : public ProcessBase
    {

    public:
        CoroProcess(Context *context, std::coroutine_handle<> root, const std::string &name = "");
        ~CoroProcess() override;
        CoroProcess(const CoroProcess &) = delete;
        CoroProcess &operator=(const CoroProcess &) = delete;

        bool done() const override;

    private:
        void resume() override;

    private:
        std::coroutine_handle<> root_;
    };
}

// Convenience macro for registering a method as a process
#define DSPSIM_METHOD(method) \
    context()->register_method<&std::remove_reference<decltype(*this)>::type::method>(this, this->hier_name() + "." + std::string(#method))

// Convenience macro for registering a coroutine task as a process
#define DSPSIM_CORO(task) \
    context()->register_coro_task(task(), this->hier_name() + "." + std::string(#task))
