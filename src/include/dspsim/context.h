#pragma once
#include <dspsim/model.h>
#include <dspsim/event.h>
#include <dspsim/process.h>
#include <dspsim/utils/unique_stack.h>
#include <dspsim/utils/flagged_stack.h>
#include <dspsim/utils/priority_queue.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace spdlog
{
    class logger;
}

namespace dspsim
{
    class Model;
    class Module;
    class ModuleName;
    class SignalBase;
    class DerivedSignalBase;

    /// Thrown when a new context is created while the active context has not been elaborated.
    class ContextConstructionError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    /*
        Context contains a vector of all the models.
        Responsible for elaboration, simulation, and management of models.

        Context states:
        - Design setup. Models can be added to the context. Models self-register with the global active context, so
          only one context can be under construction at a time. Context::create() takes a construction lock: another
          thread calling create() blocks until the context is elaborated or released, and the same thread calling
          create() again throws ContextConstructionError, since waiting would deadlock.
        - Released. Once elaboration is complete, the context is released from the global factory.
          The context can be used for simulation, but no new models can be added to it.
          New contexts can be created with new parameters so that they can run in parallel.
    */
    class Context
    {
        friend class ContextFactory;

    private:
        // Can't create context directly. Use create() to make a new global context, and obtain() to get the active one.
        Context(const std::string &name, int id);

    public:
        // Explicitly delete copy constructor and assignment
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;

        // Move operations can be kept if desired
        Context(Context &&) noexcept = default;
        ~Context();

        // Methods
        /*
            Clear all models from the context.
        */
        void clear();

        /*
            Call elaborate after construction is complete.
            This will finalize all port bindings,
            and TODO: check for any netlist violations.
            The design is then locked and the context is released from the global context factory,
            so a new, independent context can be created. The context is released even if elaboration fails.
        */
        void elaborate();

        // True once elaborate() has been called.
        bool elaborated() const;

        /*
            Stop being the global active context and release the construction lock, if this context is the active one.
            Does nothing if another context is active.
        */
        void release();
        // True while this context is the active context under construction.
        bool constructing() const;

        // Compute a single delta cycle.
        int eval();

        /*
            Run the simulation for the given time increment.
            If time_inc is 0, it will run a delta cycle without advancing time.
        */
        void run(uint64_t time_inc = 0);

        // Schedule the process to be evaluated after the given time delta relative to the current simulation time.
        void schedule_time_delta_event(uint64_t time_delta, ProcessBase *process = nullptr);

        // wait on all events in the static sensitivity list.
        Wait wait();
        // Wait py_wait() { return wait(); }
        // Wait on a time event.
        WaitTimeEvent wait(uint64_t time_delta, ProcessBase *process = nullptr);
        // WaitTimeEvent py_wait_time_event(uint64_t time_delta, ProcessBase *process = nullptr) { return wait(time_delta, process); }

        // wait on a dynamic event.
        WaitSensitivityEvent wait(SensitivityEvent &event, ProcessBase *process = nullptr);
        // WaitSensitivityEvent py_wait_sensitivity_event(SensitivityEvent &event, ProcessBase *process = nullptr) { return wait(event, process); }
        // wait on multiple dynamic events.
        WaitSensitivityEvent wait(std::vector<std::reference_wrapper<SensitivityEvent>> events, ProcessBase *process = nullptr);
        // wait on multiple dynamic events, given by pointer.
        WaitSensitivityEvent wait(const std::vector<SensitivityEvent *> &events, ProcessBase *process = nullptr);
        // WaitSensitivityEvent py_wait_sensitivity_events(std::vector<std::reference_wrapper<SensitivityEvent>> events, ProcessBase *process = nullptr) { return wait(events, process); }

        // Log the model hierarchy, starting from the given parent (nullptr = roots).
        void print_hierarchy(Model *parent = nullptr, int depth = 0) const;

        // Properties
        // Context name. Initialized when created.
        const std::string &name() const;

        // Context id. Initialized when created.
        int id() const;

        // List of all registered models in the context.
        const std::vector<Model *> &models() const;
        const std::vector<std::shared_ptr<Model>> &owned_models() const { return _owned_models; }

        // List of all registered modules in the context.
        const std::vector<Module *> &modules() const;

        // List of all registered signals in the context.
        const std::vector<SignalBase *> &signals() const;

        // Direct children of a model in the design hierarchy. Pass nullptr for the top-level (root) models.
        const std::vector<Model *> &children(Model *parent = nullptr) const;

        // Current simulation time.
        uint64_t time() const;

        // Time unit. Necessary for tracing.
        const std::string &time_unit() const;
        void set_time_unit(const std::string &time_unit);

        // Log level.
        const std::string log_level() const;
        void set_log_level(const std::string &log_level);

        // String representation of the context.
        const std::string repr() const;

        // Function to log a message at the specified log level. Needed for code that doesnt link to spdlog. (Python)
        void log(const std::string &level, const std::string &message);

        /*
            Pseudo-Private Methods.
            Not intended to be called,
            but I haven't set friend classes yet.
        */

        // Register a model with the context. This will automatically add it to the appropriate lists (modules, signals, etc.) with a dynamic_cast.
        void _add_model(Model *model);
        void _add_module(Module *module);
        void _add_signal(SignalBase *signal);

        /*
            Take shared ownership of a model. The model will stay alive as long as the context does.
            Useful in python if a design is constructed in a function and the context is returned.
        */
        void _own_model(std::shared_ptr<Model> model);
        // void _own_module(std::shared_ptr<Module> module);

        void trace_model(Model *model) { _trace_stack.push_back(model); }

        uint32_t next_event_id() { return _next_event_id++; }

        // Each model is assigned a unique id from its context on construction.
        uint32_t next_model_id() { return _next_model_id++; }

        uint32_t next_process_id() { return _next_process_id++; }
        // Register a process with the context. This will create a Process object and set it as the active process.
        ProcessBase *register_process_func(const std::function<void()> &eval, const std::string &name = "");

        template <typename MemberFunc, typename ClassType>
        ProcessBase *register_method(MemberFunc mem_ptr, ClassType *instance, const std::string &name = "")
        {
            return register_process_func(method_to_function(mem_ptr, instance), name);
        }

        ProcessBase *register_coro_task(Task task, const std::string &name = "");

        /*
            The current hierarchal module being constructed.
        */
        Module *_active_module() const;

        /*
            The current, expanded hierarchy name.
        */
        const std::string _current_hierarchy() const;

        int64_t update_count() const { return update_count_; }

    private:
        void _do_initialize();
        // Commit all scheduled signal updates, then recompute the derived signals of the signals that changed.
        void _update_signals();

    public:
        /*
            Static Methods
        */
        // Obtain the active global context. Throws ContextConstructionError if there is none.
        static std::shared_ptr<Context> obtain();
        // Set the global context to nullptr. New designs must call create().
        static void reset();
        // Create a new global context. Throws ContextConstructionError if the active context is not elaborated.
        static std::shared_ptr<Context> create(const std::string &name = "");
        // Members

    private:
        // Context info
        std::string _name;
        int _id;
        // Each model is assigned a unique ID, starting from 0.
        uint32_t _next_model_id;
        // List of all registered models, modules, and signals.
        std::vector<Model *> _registered_models;
        std::vector<Module *> _modules;
        std::vector<SignalBase *> _signals;

        // Each process is assigned a unique ID, starting from 0.
        uint32_t _next_process_id;

        uint32_t _next_event_id;

    public:
        // Processes are allocated from functions, so they need to live somewhere.
        std::vector<std::unique_ptr<ProcessBase>> _processes;

    private:
        // Owned models stay alive with context.Necessary if a design is created in a function and the context is returned.
        // More likely to be used in Python
        std::vector<std::shared_ptr<Model>> _owned_models;

        // Design hierarchy: maps a model to its direct children (root models are keyed by nullptr).
        std::unordered_map<Model *, std::vector<Model *>> _children;

        // Current simulation time.
        uint64_t _time;
        // Time unit used for tracing.
        std::string _time_unit;
        bool _initialized = false;
        // Set by elaborate(). The design is locked once elaborated.
        bool _elaborated = false;
        // Incremented every time a delta cycle occurs. Used to handle event flags.
        int64_t update_count_;

    public:
        // All processes that need to run in the current delta cycle.
        FlaggedStack<ProcessBase *> _process_eval_stack;
        ProcessBase *_current_process;

        // All signals that need to be updated in the current delta cycle.
        FlaggedStack<SignalBase *> _signal_update_stack;
        // Derived signals to recompute after the signal updates in the current delta cycle.
        FlaggedStack<DerivedSignalBase *> _derived_update_stack;
        // Scheduled sensitivity events.
        std::vector<SensitivityEvent *> _sensitivity_event_stack;
        // Scheduled time events.
        PriorityQueue<TimeEvent> _time_event_stack;
        // Stack of models that have requested tracing. Evaluated at end of a delta cycle.
        std::vector<Model *> _trace_stack;

        std::shared_ptr<spdlog::logger> logger;

        // Keep track of the currently active module to build a hierarchy.
        std::vector<Module *> _active_module_stack;
        // Names of the ModuleName scopes being constructed. Module() takes its name from the top.
        std::vector<std::string> _active_module_name_stack;
    };

    /*
        Tracks the active context, the one under construction that new models register with.
        Only one context can be under construction at a time, and only by the thread that created it.
    */
    class ContextFactory
    {
    public:
        ContextFactory();

        /*
            Obtain the active context. Throws ContextConstructionError if this thread has no context under construction:
            create() must be called first, and the context is no longer active once it has been elaborated or released.
        */
        std::shared_ptr<Context> obtain();
        // Discard the active context and release the construction lock, from any thread.
        void reset();
        // Release the construction lock, if the given context is the active one.
        void release(const Context *context);
        // True if the given context is the active one.
        bool is_active(const Context *context);
        /*
            Create a new context and make it the active one. Blocks while another thread has a context under construction.
            Throws ContextConstructionError if this thread already has one: elaborate or release it first,
            or call reset() to discard it.
        */
        std::shared_ptr<Context> create(const std::string &name = "");

    private:
        // Release the active context (and its construction lock) if the predicate accepts it.
        void release_active(const std::function<bool(const Context *)> &if_active);

    private:
        int _next_context_id;
        std::shared_ptr<Context> _active_context;
        // The thread constructing the active context.
        std::thread::id _owner;
        // Guards the active context and its owner. Never held while calling into models or contexts.
        std::mutex _mutex;
        // Notified when the active context is released.
        std::condition_variable _released;
    };

    using ContextFactoryPtr = std::shared_ptr<ContextFactory>;
    std::shared_ptr<ContextFactory> get_global_context_factory();
    void set_global_context_factory(std::shared_ptr<ContextFactory> factory);
    void reset_global_context_factory();
}
