#include "dspsim/context.h"
#include "dspsim/model.h"
// #include <fmt/format.h>
// #include <format>
#include <cmath>
#include <iostream>

namespace dspsim
{
    // static ContextFactory _global_context_factory;
    // static ContextFactoryPtr global_context_factory = &_global_context_factory;
    // static ContextFactoryPtr global_context_factory = nullptr;

    Context::Context()
    {
        m_time = 0;
        m_elaborate_done = false;

        static int global_id = 0;
        m_id = global_id++;
    }

    Context::~Context()
    {
        // std::cout << fmt::format("Clearing Context: {}", (intptr_t)this) << std::endl;
        clear();
    }

    void Context::own_model(std::shared_ptr<Model> model)
    {
        m_owned_models.push_back(model);
    }
    void Context::register_model(Model *model)
    {
        m_models.push_back(model);
    }

    void Context::set_timescale(double _time_unit, double _time_precision)
    {
        m_time_unit = _time_unit;
        m_time_precision = _time_precision;

        // Require that the time_precision be higer resolution than the time unit. Only using powers of ten.
        m_time_step = m_time_unit / m_time_precision;
    }

    void Context::elaborate()
    {
        // m_models = std::vector<ModelBase *>(m_owned_models.begin(), m_owned_models.end());
        m_elaborate_done = true;
        // global_context_factory->reset_context();
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

    void Context::advance(uint64_t _time_inc)
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

    // // Used as a context manager in python
    // void Context::enter_context(double time_unit, double time_precision)
    // {
    //     set_timescale(time_unit, time_precision);
    // }
    // void Context::exit_context()
    // {
    //     clear();
    //     // Context::detach();
    // }

    std::string Context::print_info()
    {
        auto s = std::string("Context(");
        s += "id=" + std::to_string(m_id) + ", ";
        s += "time=" + std::to_string(m_time) + ", ";
        s += "n_models=" + std::to_string(m_models.size()) + ", ";
        s += "n_registered=" + std::to_string(m_owned_models.size()) + ", ";
        s += "n_time_unit=" + std::to_string(m_time_unit) + ", ";
        s += "n_time_precision=" + std::to_string(m_time_precision) + ", ";
        s += "n_time_step=" + std::to_string(m_time_step) + ", ";
        s += "this=" + std::to_string((intptr_t)this) + ", ";
        return s;
        // return fmt::format("Context(id={}, time={}, n_models={}, n_registered={}, time_unit={}, time_precision={}, time_step={}, this={})",
        //                    m_id, m_time, m_models.size(), m_owned_models.size(), m_time_unit, m_time_precision, m_time_step, (intptr_t)this);
    }

    // ///////////
    // std::shared_ptr<Context> Context::context(std::shared_ptr<Context> new_context, bool detach)
    // {
    //     // static std::shared_ptr<Context> global_context{new Context};
    //     static std::shared_ptr<Context> global_context = nullptr;

    //     if (detach)
    //     {
    //         global_context = nullptr;
    //         return global_context;
    //     }
    //     if (new_context)
    //     {
    //         global_context = new_context;
    //     }
    //     if (global_context == nullptr)
    //     {
    //         global_context = std::shared_ptr<Context>{new Context};
    //     }
    //     return global_context;
    // }
    // ContextPtr Context::context(std::shared_ptr<Context> new_context)
    // {
    //     static ContextPtr global_context = nullptr;
    //     if (new_context)
    //     {
    //         global_context = new_context;
    //     }
    //     if (global_context == nullptr)
    //     {
    //         global_context = std::shared_ptr<Context>{new Context};
    //     }
    //     return global_context;
    // }
    ContextPtr Context::create(double time_unit, double time_precision)
    {
        // std::shared_ptr<Context> new_context{new Context};
        // new_context->set_timescale(time_unit, time_precision);
        // return Context::context(new_context);

        auto new_context = global_context_factory()->create();
        new_context->set_timescale(time_unit, time_precision);
        return new_context;
    }
    // ContextPtr Context::create(double time_unit, double time_precision)
    // {
    //     ContextPtr context = get_global_context_factory()->get_context();
    //     // context->configure(time_unit, time_precision);
    //     context->set_timescale(time_unit, time_precision);
    //     return context;
    // }
    ContextPtr Context::obtain()
    {
        // return Context::context();
        return global_context_factory()->obtain();
    }

    ContextFactory::ContextFactory()
    {
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

    ContextFactory *global_context_factory(ContextFactory *new_context_factory)
    {
        static ContextFactory _default_context_factory;
        static ContextFactory *static_context_factory = &_default_context_factory;

        if (new_context_factory)
        {
            static_context_factory = new_context_factory;
        }

        return static_context_factory;
    }
    void set_global_context_factory(ContextFactory *new_context_factory)
    {
        global_context_factory(new_context_factory);
    }
    ContextFactory *get_global_context_factory()
    {
        return global_context_factory();
    }
} // namespace dspsim
