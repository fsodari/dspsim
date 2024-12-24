#include "dspsim/context.h"
#include "dspsim/model.h"
#include <format>
#include <cmath>
#include <iostream>

namespace dspsim
{
    Context::Context()
    {
        m_time = 0;
        m_elaborate_done = false;

        static int global_id = 0;
        m_id = global_id++;
    }

    Context::~Context()
    {
        clear();
    }

    // void Context::_register_model(std::shared_ptr<Model> model)
    // {
    //     m_owned_models.push_back(model);
    // }
    // void Context::register_model(Model *model)
    // {
    //     m_models.push_back(model);
    // }

    void Context::set_timescale(double _time_unit, double _time_precision)
    {
        m_time_unit = _time_unit;
        m_time_precision = _time_precision;

        // Require that the time_precision be higer resolution than the time unit. Only using powers of ten.
        m_time_step = m_time_unit / m_time_precision;
    }

    void Context::elaborate()
    {
        m_elaborate_done = true;
    }

    void Context::clear()
    {
        m_owned_models.clear();
        m_models.clear();

        m_time = 0;
        m_elaborate_done = false;
    }

    void Context::eval() const
    {
        for (auto const &m : m_models)
        {
            m->eval_step();
        }
        for (auto const &m : m_models)
        {
            m->eval_end_step();
        }
    }

    void Context::run(uint64_t _time_inc)
    {
        // The number of steps in time_precision.
        uint64_t n_steps = _time_inc * m_time_step;
        for (uint64_t i = 0; i < n_steps; i++)
        {
            // Run the eval loop.
            eval();
            // Increment the time.
            ++m_time;
        }
    }
    std::vector<Model *> &Context::models()
    {
        return m_models;
    }

    std::string Context::_repr() const
    {
        return std::format("Context(id={}, time={}, n_models={}, n_registered={}, time_unit={}, time_precision={}, time_step={}, this={})",
                           m_id,
                           m_time,
                           m_models.size(),
                           m_owned_models.size(),
                           m_time_unit,
                           m_time_precision,
                           m_time_step, (intptr_t)this);
    }

    ContextPtr Context::create(double time_unit, double time_precision)
    {
        auto new_context = get_global_context_factory().create();
        new_context->set_timescale(time_unit, time_precision);
        return new_context;
    }

    ContextPtr Context::obtain()
    {
        return get_global_context_factory().obtain();
    }

    ContextPtr ContextFactory::create()
    {
        _active_context = std::make_shared<Context>();
        return _active_context;
    }
    ContextPtr ContextFactory::obtain()
    {
        // If a context hasn't been created yet, create a context.
        if (_active_context == nullptr)
        {
            _active_context = create();
        }
        return _active_context;
    }

    static ContextFactory *global_context_factory(ContextFactory *new_context_factory = nullptr)
    {
        static ContextFactory _default_context_factory;
        static ContextFactory *static_context_factory = &_default_context_factory;

        if (new_context_factory)
        {
            static_context_factory = new_context_factory;
        }

        return static_context_factory;
    }

    void set_global_context_factory(ContextFactory &new_context_factory)
    {
        global_context_factory(&new_context_factory);
    }
    ContextFactory &get_global_context_factory()
    {
        return *global_context_factory();
    }
} // namespace dspsim
