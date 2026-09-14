#pragma once
#include <memory>
#include <vector>
#include <string>

namespace dspsim
{
    // Forward declaration of Model class
    // class Model;
    using ModelPtr = std::shared_ptr<class Model>;
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

        // Model can access the context's next model id.
        friend class Model;
        // Simulator can access the context's time.
        friend class Simulator;

    private:
        Context();

    public:
        ~Context();

        /*
            Properties
        */

        // Context id
        int id() const { return _id; }

        const std::vector<ModelPtr> &models() const { return _models; }

        uint64_t time() const { return _time; }
        const std::string &time_unit() const { return _time_unit; }
        const std::string &time_precision() const { return _time_precision; }

        /*
            Methods
        */
        void set_timescale(const std::string &time_unit, const std::string &time_precision);

        //
        const std::string repr() const;

        /*
            Methods
        */
        // Register a model with the context.
        void register_model(ModelPtr model);

        // Clear all models from the context.
        void clear();

        void eval();
        void run(uint64_t time_inc);

    private:
        int get_next_model_id();

    public:
        /*
            Static Methods
        */
        // Obtain the global context.
        static ContextPtr obtain();

        // Set the global context to nullptr. New designs will create a new context.
        static void reset_global_context();

    private:
        int _id;
        int _next_model_id;
        std::vector<ModelPtr> _models;
        uint64_t _time;
        std::string _time_unit;
        std::string _time_precision;
        uint64_t _time_step;
    };

    class ContextFactory
    {
    public:
        ContextFactory();

        // Obtain the current active context
        ContextPtr obtain();
        // Reset the active context.
        void reset();

    private:
        int _next_context_id;
        ContextPtr _active_context;
    };

    using ContextFactoryPtr = std::shared_ptr<ContextFactory>;
    ContextFactoryPtr get_global_context_factory();
    void set_global_context_factory(ContextFactoryPtr factory);
    void reset_global_context_factory();
}
