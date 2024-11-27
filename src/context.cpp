#include "dspsim/context.h"
#include "dspsim/model.h"
#include <cmath>

namespace dspsim
{
    Context::Context(int time_unit, int time_precision)
    {
        m_time = 0;
        set_timescale(time_unit, time_precision);

        m_elaborate_done = false;
    }

    void Context::register_model(std::shared_ptr<ModelBase> model)
    {
        m_owned_models.push_back(model);
    }
    void Context::register_model(ModelBase *model)
    {
        m_unowned_models.push_back(model);
    }

    void Context::set_timescale(int _time_unit, int _time_precision)
    {
        m_time_unit = abs(_time_unit);
        m_time_precision = abs(_time_precision);

        // Require that the time_precision be higer resolution than the time unit. Only using powers of ten.
        m_time_step = std::pow(10, (m_time_precision - m_time_unit));
    }

    void Context::elaborate()
    {
        m_models = std::vector<ModelBase *>(m_owned_models.begin(), m_owned_models.end());
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

    ///////////
    std::shared_ptr<Context> Context::context(std::shared_ptr<Context> new_context = nullptr)
    {
        static std::shared_ptr<Context> global_context{new Context};
        if (new_context)
        {
            global_context = new_context;
        }
        return global_context;
    }
    std::shared_ptr<Context> Context::new_global_context(int time_unit = 9, int time_precision = 9)
    {
        std::shared_ptr<Context> new_context{new Context(time_unit, time_precision)};
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
