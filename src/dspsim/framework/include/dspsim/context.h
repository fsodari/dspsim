#pragma once
#include <dspsim/forward.h>
#include <dspsim/event.h>
#include <dspsim/utils/unique_stack.h>
#include <dspsim/utils/sorted_stack.h>
#include <memory>
#include <vector>
#include <string>
#include <deque>
#include <unordered_set>
#include <set>
// #include <spdlog/spdlog.h>
namespace spdlog
{
    class logger;
}

namespace dspsim
{
    // Forward declaration of Model class
    // class Model;
    // class Module;
    // class ModuleName;
    using ModelPtr = std::shared_ptr<Model>;
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

    public:
        Context(int id = -1);
        ~Context();

        /*
            Properties
        */

        // Context id
        int id() const { return _id; }

        const std::vector<Model *> &models() const { return _registered_models; }

        uint64_t time() const { return _time; }
        const std::string &time_unit() const { return _time_unit; }
        // const std::string &time_precision() const { return _time_precision; }

        /*
            Methods
        */
        // void set_timescale(const std::string &time_unit, const std::string &time_precision);

        //
        const std::string repr() const;

        /*
            Methods
        */
        // Register a model with the context.
        void add_model(Model *model);
        // void register_signal(SignalBase *signal);

        // Take shared ownership of a model. The model will stay alive as long as the context does.
        void own_model(ModelPtr model);

        // Clear all models from the context.
        void clear();

        //
        void elaborate();

        // Compute a single delta cycle.
        int eval();
        // Running with time_inc = 0 will run a delta cycle without advancing time.
        void run(uint64_t time_inc = 0);

        void _push_eval_stack(Model *model) { _eval_stack.push(model); }
        // void _push_update_stack(Model *model) { _update_stack.push(model); }
        void _push_time_event_stack(TimeEvent event) { _time_event_stack.push(event); }

    public:
        /*
            Static Methods
        */
        // Obtain the global context.
        static ContextPtr obtain();
        // Set the global context to nullptr. New designs will create a new context.
        static void reset();
        // Reset the global context, then obtain a new one.
        static ContextPtr create();

        Module *active_module() const { return _active_module_stack.empty() ? nullptr : _active_module_stack.front(); }

        const std::string hier() const;

    public:
        std::shared_ptr<spdlog::logger> logger;
        std::deque<Module *> _active_module_stack;
        std::deque<ModuleName *> _active_module_name_stack;

    private:
        int _id;
        size_t _next_model_id;
        std::vector<Model *> _registered_models;
        // Owned models stay alive with context.
        std::vector<ModelPtr> _owned_models;
        //
        UniqueStack<Model *> _eval_stack;
        UniqueStack<Model *> _update_stack;
        SortedStack<TimeEvent> _time_event_stack;

        uint64_t _time;
        std::string _time_unit;

        // Model *_active_model;
    };

    class ContextFactory
    {
    public:
        ContextFactory();

        // Obtain the current active context
        ContextPtr obtain();
        // Reset the active context.
        void reset();
        // Reset the global context, then obtain a new one.
        ContextPtr create();

    private:
        int _next_context_id;
        ContextPtr _active_context;
    };

    using ContextFactoryPtr = std::shared_ptr<ContextFactory>;
    ContextFactoryPtr get_global_context_factory();
    void set_global_context_factory(ContextFactoryPtr factory);
    void reset_global_context_factory();
}
