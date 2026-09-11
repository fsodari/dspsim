#pragma once
#include <memory>
#include <vector>

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

        double time() const { return _time; }

        /*
            Methods
        */
        // Register a model with the context.
        void register_model(ModelPtr model);

        // Clear all models from the context.
        void clear();

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
        double _time;
    };
}
