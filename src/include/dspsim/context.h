#pragma once
#include <memory>
#include <list>
#include <vector>
#include <cstdint>

namespace dspsim
{
    // Forward declaration of Model base. Needed by Context.
    class ModelBase;
    class Context;

    // ContextPtr.
    using ContextPtr = std::shared_ptr<Context>;

    class Context
    {
        /*
            Context is a singleton class, when initialized
        */

        Context(Context const &) = delete;
        Context &operator=(Context const &) = delete;
        Context(int time_unit = 9, int time_precision = 9);

    public:
        // Register a model with the context. The context will own the model.
        void register_model(std::shared_ptr<ModelBase> model);
        void register_model(ModelBase *model);

        void set_timescale(int time_unit = 9, int time_precision = 9);

        // Read and write the time_unit. Writing to the time_unit will update the timescale.
        int time_unit() const { return m_time_unit; }
        void set_time_unit(int _time_unit) { set_timescale(_time_unit, m_time_precision); }

        // Read and write the time_precision. Writing to the time_unit will update the time_precision.
        int time_precision() const { return m_time_precision; }
        void set_time_precision(int _time_precision) { set_timescale(m_time_unit, _time_precision); }

        // return the time_step. Clocks and other real-time sensitive models will need to know this.
        int time_step() const { return m_time_step; }

        void elaborate();

        // Indicates that elaboration has been run.
        bool elaborate_done() const { return m_elaborate_done; }

        // Clear all references to models, reset the clock, (reset verilated context?)
        void clear();

        uint64_t time() const { return m_time / m_time_step; }

        // Run the eval_step, eval_end_step cycle.
        void eval() const;

        // Advance the time in units of the time_unit and run the eval_step in increments of time_precision
        void advance(uint64_t time_inc);

        // Return a reference to the list of all registered models.
        std::vector<ModelBase *> &models();

    public:
        // Context factory functions.
        // Get the current global context. If a context is given, set the global context to new_context.
        static std::shared_ptr<Context> context(std::shared_ptr<Context> new_context = nullptr);
        // Create a new global context and return it.
        static std::shared_ptr<Context> new_global_context(int time_unit = 9, int time_precision = 9);

        // Helpers for python properties.
        // Get the global context.
        static std::shared_ptr<Context> get_global_context();
        static void set_global_context(std::shared_ptr<Context> new_context);

    private:
        // The vector containing all simulation models. This is generated during the elaboration step from m_unowned_models.
        // We could just make m_unowned_models a vector and use it directly. Adding elements one by one to a vector isn't ideal?
        std::vector<ModelBase *> m_models;

        // If a model was created with a constructor call, it can't be automatically registered as a shared ptr.
        // It will only be alive as long as it's in scope. You must make sure the models are alive as long as the context is alive.
        std::list<ModelBase *> m_unowned_models;

        // If a model was created as a shared ptr, the context will keep a copy. That way the model stays alive as long as the context is alive.
        std::list<std::shared_ptr<ModelBase>> m_owned_models;

        // Context time.
        uint64_t m_time = 0;
        int m_time_unit = 9, m_time_precision = 9, m_time_step = 1;

        // When a context is done elaborating, no new models can be registered. The global context can be reset and another context may be created.
        bool m_elaborate_done = false;
    };

} // namespace dspsim
