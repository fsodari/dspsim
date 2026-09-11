#pragma once
#include <memory>
#include <vector>

namespace dspsim
{
    // Forward declaration of Model class
    class Model;
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
        Context();
        ~Context();

        // Return a string representation of the context.
        const std::string repr() const;

        // Register a model with the context.
        void register_model(std::shared_ptr<Model> model);

        void clear();
        // Complete the design. Assign ids to every model.
        void elaborate();

        // Run an eval cycle on all models in the context.
        void eval();

        int id() const { return _id; }

        const std::vector<std::shared_ptr<Model>> &models() const { return _models; }

        // Reset the global context to a new context.
        static ContextPtr reset();
        // Obtain the global context.
        static ContextPtr obtain();

        static ContextPtr global_context();

    private:
        int _id;
        int _next_model_id;
        std::vector<std::shared_ptr<Model>> _models;
    };
}
