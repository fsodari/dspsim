#include "dspsim/context.h"
#include "dspsim/model.h"
#include <fmt/format.h>
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
        std::cout << fmt::format("Clearing Context: {}", (intptr_t)this) << std::endl;
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
        this->detach();

        eval();
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

    // Used as a context manager in python
    void Context::enter_context(double time_unit, double time_precision)
    {
        set_timescale(time_unit, time_precision);
    }
    void Context::exit_context()
    {
        clear();
        Context::detach();
    }

    std::string Context::print_info()
    {
        return fmt::format("Context(id={}, time={}, n_models={}, n_registered={}, time_unit={}, time_precision={}, time_step={}, this={})",
                           m_id, m_time, m_models.size(), m_owned_models.size(), m_time_unit, m_time_precision, m_time_step, (intptr_t)this);
    }

    ///////////
    std::shared_ptr<Context> Context::context(std::shared_ptr<Context> new_context, bool detach)
    {
        // static std::shared_ptr<Context> global_context{new Context};
        static std::shared_ptr<Context> global_context = nullptr;

        if (detach)
        {
            global_context = nullptr;
            return global_context;
        }
        if (new_context)
        {
            global_context = new_context;
        }
        if (global_context == nullptr)
        {
            global_context = std::shared_ptr<Context>{new Context};
        }
        return global_context;
    }
    std::shared_ptr<Context> Context::create(double time_unit, double time_precision)
    {
        std::shared_ptr<Context> new_context{new Context};
        new_context->enter_context(time_unit, time_precision);
        return Context::context(new_context);
    }

    std::shared_ptr<Context> Context::get_global_context()
    {
        return Context::context();
    }
    void Context::set_global_context(std::shared_ptr<Context> new_context)
    {
        Context::context(new_context);
    }
} // namespace dspsim
