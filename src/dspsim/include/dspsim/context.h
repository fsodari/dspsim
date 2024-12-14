#pragma once
#include "dspsim/units.h"

#include <memory>
#include <list>
#include <vector>
#include <string>
#include <cstdint>

namespace dspsim
{
    // Forward declaration of Model base. Needed by Context.
    class Model;
    class Context;

    // ContextPtr.
    using ContextPtr = std::shared_ptr<Context>;

    class Context
    {
    public:
        Context();
        ~Context();

        // Give ownership of a model to the context. The model will stay alive as long as the context is alive.
        void own_model(std::shared_ptr<Model> model);

        // Register a model with the context. Registered models will be evaluated in the eval loop.
        void register_model(Model *model);

        // time_unit must be an integer multiple of time_precision.
        void set_timescale(double time_unit = units::ns(1.0), double time_precision = units::ns(1.0));

        // Read and write the time_unit. Writing to the time_unit will update the timescale.
        double time_unit() const { return m_time_unit; }
        void set_time_unit(double _time_unit) { set_timescale(_time_unit, m_time_precision); }

        // Read and write the time_precision. Writing to the time_unit will update the time_precision.
        double time_precision() const { return m_time_precision; }
        void set_time_precision(double _time_precision) { set_timescale(m_time_unit, _time_precision); }

        // return the time_step. Clocks and other real-time sensitive models will need to know this.
        int time_step() const { return m_time_step; }

        /*
            Once elaboration is finished, no more models are allowed to be instantiated.
            At this point, the context may be 'detached' from the active global context.

            At this step in the process, we can also perform DRC/ERC to ensure signals are
            connected properly, there are no multiply driven signals, etc. The model list
            can be compiled into a vector, or other optimizations can be made.

            The application cannot advance the simulation until elaborate has been completed.

            Applications using Runners in multiple threads will need to have a mutex
            to allow only one Runner to set up at a time, once elaboration is complete, the
            mutex is released and the context detached. Other context's
        */
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
        std::vector<Model *> &models();

        // // Used as a context manager in python
        // void enter_context(double time_unit = units::ns(1.0), double time_precision = units::ns(1.0));
        // void exit_context();

    public:
        // // Context factory functions.
        // // Get the current global context. If a context is given, set the global context to new_context.
        // static std::shared_ptr<Context> context(std::shared_ptr<Context> new_context = nullptr, bool detach = false);
        // // Create a new global context and return it.
        // static std::shared_ptr<Context> create(double time_unit = units::ns(1.0), double time_precision = units::ns(1.0));

        // Create and configure a new context and reset the global active_context.
        // This uses the global_context_factory to create a context
        static ContextPtr create(double time_unit = 1e-9, double time_precision = 1e-9);
        // Obtain the active context from the global context factory.
        static ContextPtr obtain();
        // static ContextPtr context(std::shared_ptr<Context> new_context = nullptr);

        std::string print_info();

    private:
        // The vector containing all simulation models. This is generated during the elaboration step from m_unowned_models.
        // We could just make m_unowned_models a vector and use it directly. Adding elements one by one to a vector isn't ideal?
        std::vector<Model *> m_models;

        // // If a model was created with a constructor call, it can't be automatically registered as a shared ptr.
        // // It will only be alive as long as it's in scope. You must make sure the models are alive as long as the context is alive.
        // std::list<ModelBase *> m_unowned_models;

        // If a model was created as a shared ptr, the context will keep a copy. That way the model stays alive as long as the context is alive.
        std::list<std::shared_ptr<Model>> m_owned_models;

        // Context time.
        uint64_t m_time = 0;
        double m_time_unit = units::ns(1.0), m_time_precision = units::ns(1.0);
        int m_time_step = 1;

        // When a context is done elaborating, no new models can be registered. The global context can be reset and another context may be created.
        bool m_elaborate_done = false;
        int m_id = 0;
    };

    /*

    */
    class ContextFactory
    {
    public:
        ContextFactory();

        ContextPtr create();
        ContextPtr obtain();
        void reset() { _active_context.reset(); }

    private:
        ContextPtr _active_context;
    };

    ContextFactory *global_context_factory(ContextFactory *new_context_factory = nullptr);
    void set_global_context_factory(ContextFactory *new_context_factory);
    ContextFactory *get_global_context_factory();

    // /*
    //     If separate modules statically link to dspsim-core, then they will have independent global variables and/or
    //     static state. We will use a ContextFactory to create or obtain the context.
    //     Modules can then set their global_context_factory to the dspsim.framework's ContextFactory.

    //     A python module's __init__.py should contain these lines:

    //         import dspsim
    //         from my_module._my_library import set_context_factory
    //         set_context_factory(dspsim.context_factory())

    //     When the library is imported, it will sync its global_context_factory, and nothing more needs to be done.

    //     The ContextFactory will have methods to either obtain the current context, or reset the active Context.
    //     The ContextFactory contains a shared_ptr to the active Context. When Models are constructed from the factory,
    //     they will be registered with the factory's active_context.

    //     When a new context is created, the active_context is replaced. Models created under the old context
    //     will still have a reference to the context they were created with. Any application that still has a
    //     reference to the old Context will keep the Context and its Models alive as long as it is in scope.
    // */
    // class ContextFactory
    // {
    // public:
    //     ContextFactory();

    //     /*
    //         Obtain a shared_ptr to the active Context. This is used when initializing a Context variable in an application.
    //     */
    //     ContextPtr get_context() const;

    //     /*
    //         Create a new context, replacing the active context.
    //         When context->elaborate() is called, it will call reset_context() at the end of the function call.
    //      */
    //     void reset_context();

    //     /*
    //         Get an unowned ptr to the active context. The model base class uses this when registering to the context.
    //     */
    //     Context *context() const;

    // private:
    //     ContextPtr m_active_context = nullptr;
    // };

    // // Does this need to be shared?
    // // using ContextFactoryPtr = ContextFactory *;
    // using ContextFactoryPtr = std::shared_ptr<ContextFactory>;

    // // The global context factory. Library modules will set their global_context_factories to the framework factory.
    // ContextFactoryPtr get_global_context_factory();
    // void set_global_context_factory(ContextFactoryPtr context);
    // void reset_global_context_factory();

} // namespace dspsim
