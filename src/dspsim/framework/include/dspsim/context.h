#pragma once
#include <dspsim/forward.h>
#include <dspsim/model.h>
#include <dspsim/signal.h>
#include <dspsim/event.h>
#include <dspsim/utils/unique_stack.h>
#include <dspsim/utils/priority_queue.h>
#include <memory>
#include <vector>
#include <string>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <set>

namespace spdlog
{
    class logger;
}

namespace dspsim
{
    // using ModelPtr = std::shared_ptr<Model>;
    // using ModulePtr = std::shared_ptr<Module>;
    using ContextPtr = std::shared_ptr<class Context>;
    /*
        Context contains a vector of all the models.
        Responsible for elaboration, simulation, and management of models.

        A context can have 3 states:
        - No active design. Waiting to be used. A context must be created with new parameters.
        - Design setup. Models can be added to the context. Models will self-register with the global context, so this
          must be locked to prevent other threads from adding models to the context while it is being set up.
        - Detached. Once elaboration is complete, the context is detached from the global context.
          The context can be used for simulation, but no new models can be added to it.
          New contexts can be created with new parameters so that they can run in parallel.
    */

    class Context
    {
        friend class ContextFactory;
        // Members
    private:
        std::string _name;
        int _id;
        uint32_t _next_model_id;
        std::vector<Model *> _registered_models;
        std::vector<Module *> _modules;
        std::vector<SignalBase *> _signals;

        // Owned models stay alive with context.
        std::vector<ModelPtr> _owned_models;
        std::vector<ModulePtr> _owned_modules;
        // Design hierarchy: maps a model to its direct children (root models are keyed by nullptr).
        std::unordered_map<Model *, std::vector<Model *>> _children;
        //
        UniqueStack<Model *> _eval_stack;
        // UniqueStack<Model *> _update_stack; // Signals are the only models that ever need to be in the update cycle.
    public:
        UniqueStack<SignalBase *> _signal_update_stack;

    private:
        PriorityQueue<TimeEvent> _time_event_stack;

        uint64_t _time;
        std::string _time_unit;

        /*
        Pseudo-private members that are not intended
        to be accessed publicly. Need to set up friend classes.
        */
    public:
        std::shared_ptr<spdlog::logger> logger;
        std::deque<Module *> _active_module_stack;
        std::deque<ModuleName *> _active_module_name_stack;
        bool _signal_event;

    private:
        // Can't create context directly. Must use obtain() or create() to get global context.
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

        // Direct children of a model in the design hierarchy. Pass nullptr for the top-level (root) models.
        const std::vector<Model *> &children(Model *parent = nullptr) const;

        // Current simulation time.
        uint64_t time() const;

        // Time unit. Necessary for tracing.
        const std::string &time_unit() const;
        void set_time_unit(const std::string &time_unit);

        //
        const std::string log_level() const;
        void set_log_level(const std::string &log_level);
        const std::string repr() const;

        void log(const std::string &level, const std::string &message);

        /*
            Pseudo-Private Methods.
            Not intended to be called,
            but I haven't set friend classes yet.
        */

        // Register a model with the context.
        void _add_model(Model *model);
        // Register a signal with the context.
        void _add_signal(SignalBase *signal);

        /*
            Take shared ownership of a model. The model will stay alive as long as the context does.
            Useful in python if a design is constructed in a function and the context is returned.
        */
        void _own_model(ModelPtr model);
        void _own_module(ModulePtr module);

        /*
            Schedule a model for evaluation in the next delta cycle.
        */
        void _push_eval_stack(Model *model);

        /*
            Schedule a time event. The attached module will be evaluated when the
            simulation reaches the specified time.
        */
        void _push_time_event_stack(TimeEvent event);

        /*
            The current hierarchal module being constructed.
        */
        Module *_active_module() const;

    private:
        /*
            The current, expanded hierarchy name.
        */
        const std::string _current_hierarchy() const;

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
