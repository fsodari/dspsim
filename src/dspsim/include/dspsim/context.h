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
        // Model can access the context's protected/private members so that models will register themselves automatically.
        friend class Model;

    public:
        Context();
        ~Context();

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
        void run(uint64_t time_inc);

        // Return a reference to the list of all registered models.
        std::vector<Model *> &models();

        // repr and str
        std::string _repr() const;
        std::string _str() const { return _repr(); }

        // Create and configure a new context using the global context factory.
        static ContextPtr create(double time_unit = 1e-9, double time_precision = 1e-9);
        // Obtain the active context from the global context factory.
        static ContextPtr obtain();

    private:
        // The vector containing all simulation models. This is generated during the elaboration step from m_unowned_models.
        std::vector<Model *> m_models;

        // If a model was created as a shared ptr, the context will keep a copy. That way the model stays alive as long as the context is alive.
        std::list<std::shared_ptr<Model>> m_owned_models;

        // Context time.
        uint64_t m_time = 0;
        double m_time_unit = units::ns(1.0), m_time_precision = units::ns(1.0);
        int m_time_step = 1;

        // When a context is done elaborating, no new models can be registered. The global context can be detached and another context may be created.
        bool m_elaborate_done = false;
        int m_id = 0;
    };

    /*

    */
    class ContextFactory
    {
    public:
        ContextPtr create();
        ContextPtr obtain();
        void reset() { _active_context.reset(); }

    private:
        ContextPtr _active_context;
    };

    void set_global_context_factory(ContextFactory &new_context_factory);
    ContextFactory &get_global_context_factory();

} // namespace dspsim
