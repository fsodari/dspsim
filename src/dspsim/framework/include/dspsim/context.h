#pragma once
#include <dspsim/forward.h>
#include <dspsim/model.h>
#include <dspsim/event.h>
#include <dspsim/process.h>
#include <dspsim/utils/unique_stack.h>
#include <dspsim/utils/flagged_stack.h>
#include <dspsim/utils/priority_queue.h>
#include <dspsim/utils/stack.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>

namespace spdlog
{
    class logger;
}

namespace dspsim
{

    /*
        Context contains a vector of all the models.
        Responsible for elaboration, simulation, and management of models.

        Context states:
        - Design setup. Models can be added to the context. Models will self-register with the global context, so this
          must be locked to prevent other threads from adding models to the context while it is being set up.
        - Detached. Once elaboration is complete, the context is detached from the global context.
          The context can be used for simulation, but no new models can be added to it.
          New contexts can be created with new parameters so that they can run in parallel.
    */
    using ContextPtr = std::shared_ptr<class Context>;
    class Context
    {
        friend class ContextFactory;
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
        // Processes are allocated from functions, so they need to live somewhere.
        std::vector<std::shared_ptr<Process>> _processes;

        // Owned models stay alive with context.Necessary if a design is created in a function and the context is returned.
        // More likely to be used in Python
        std::vector<ModelPtr> _owned_models;
        std::vector<ModulePtr> _owned_modules;

        // Design hierarchy: maps a model to its direct children (root models are keyed by nullptr).
        std::unordered_map<Model *, std::vector<Model *>> _children;

        // Current simulation time.
        uint64_t _time;
        // Time unit used for tracing.
        std::string _time_unit;
        bool _initialized = false;

    public:
        // All processes that need to run in the current delta cycle. Can hold every
        // process in the design, so it's given a larger initial capacity than the default.
        FlaggedStack<Process *> _process_eval_stack;
        // All signals that need to be updated in the current delta cycle. Same reasoning.
        FlaggedStack<SignalBase *> _signal_update_stack;
        // Scheduled sensitivity events.
        std::vector<SensitivityEvent *> _sensitivity_event_stack;
        // Scheduled time events.
        PriorityQueue<TimeEvent> _time_event_stack;

        std::vector<Model *> _trace_stack;
        // Flag indicating if there has been a signal event in the current delta cycle.
        // All signal event flags are cleared at the start of every delta cycle.
        bool _signal_event;

        std::shared_ptr<spdlog::logger> logger;

        // Keep track of the currently active module to build a hierarchy.
        std::vector<Module *> _active_module_stack;
        // ModuleName is used as a way of building the module hierarchy and running cleanup when Module construction ends.
        std::vector<ModuleName *> _active_module_name_stack;

    private:
        // Can't create context directly. Must use obtain() to get global context, or create() to make a new global context.
        Context(const std::string &name, int id);

    public:
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
        */
        void elaborate();

        // Compute a single delta cycle.
        int eval();

        /*
            Run the simulation for the given time increment.
            If time_inc is 0, it will run a delta cycle without advancing time.
        */
        void run(uint64_t time_inc = 0);

        // Log the model hierarchy, starting from the given parent (nullptr = roots).
        void print_hierarchy(Model *parent = nullptr, int depth = 0) const;

        // Properties
        // Context name. Initialized when created.
        const std::string &name() const;

        // Context id. Initialized when created.
        int id() const;

        // List of all registered models in the context.
        const std::vector<Model *> &models() const;

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
        void _own_model(ModelPtr model);
        void _own_module(ModulePtr module);

        void trace_model(Model *model) { _trace_stack.push_back(model); }

        // Register a process with the context. This will create a Process object and set it as the active process.
        Process *register_process_func(const std::function<void()> &eval, Model *source, const std::string &name = "");

        template <typename MemberFunc, typename ClassType>
        Process *register_method(MemberFunc mem_ptr, ClassType *instance, const std::string &name = "")
        {
            return register_process_func(method_to_function(mem_ptr, instance), static_cast<Model *>(instance), name);
        }

        /*
            The current hierarchal module being constructed.
        */
        Module *_active_module() const;

        /*
            The current, expanded hierarchy name.
        */
        const std::string _current_hierarchy() const;

    private:
        void _do_initialize();

    public:
        /*
            Static Methods
        */
        // Obtain the global context.
        static ContextPtr obtain();
        // Set the global context to nullptr. New designs will create a new context.
        static void reset();
        // Reset the global context, then obtain a new one.
        static ContextPtr create(const std::string &name = "");
    };

    class ContextFactory
    {
    private:
        int _next_context_id;
        ContextPtr _active_context;

    public:
        ContextFactory();

        // Obtain the current active context
        ContextPtr obtain();
        // Reset the active context.
        void reset();
        // Reset the global context, then obtain a new one.
        ContextPtr create(const std::string &name = "");
    };

    using ContextFactoryPtr = std::shared_ptr<ContextFactory>;
    ContextFactoryPtr get_global_context_factory();
    void set_global_context_factory(ContextFactoryPtr factory);
    void reset_global_context_factory();
}
