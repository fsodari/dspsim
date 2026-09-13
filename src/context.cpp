#include <dspsim/context.h>
#include <dspsim/model.h>
#include "timestrings.h"
#include <format>

namespace dspsim
{
    // Global context pointer. Models will self-register with the global context.
    static ContextPtr _global_context = nullptr;

    // Initialize each new context with a new id.
    static int next_context_id = 0;

    Context::Context()
        : _id(next_context_id++),
          _next_model_id(0),
          _time(0),
          _time_unit("1ns"),
          _time_precision("1ns")
    {
        set_timescale(_time_unit, _time_precision);
    }

    Context::~Context()
    {
        clear();
    }

    void Context::register_model(ModelPtr model)
    {
        _models.push_back(model);
    }

    void Context::clear()
    {
        _models.clear();
    }

    void Context::eval()
    {
        for (auto &model : _models)
        {
            model->eval_step();
        }
        for (auto &model : _models)
        {
            model->eval_end_step();
        }
    }

    void Context::run(uint64_t time_inc)
    {
        uint64_t n_steps = static_cast<uint64_t>(time_inc * _time_step);
        for (uint64_t i = 0; i < n_steps; ++i)
        {
            eval();
            _time++;
        }
    }

    int Context::get_next_model_id()
    {
        return _next_model_id++;
    }

    const std::string Context::repr() const
    {
        return std::format("Context(id={}, time={})", _id, _time);
    }

    void Context::set_timescale(const std::string &time_unit, const std::string &time_precision)
    {
        _time_unit = time_unit;
        _time_precision = time_precision;

        _time_step = calc_time_step(_time_unit, _time_precision);
    }

    ContextPtr Context::obtain()
    {
        if (_global_context == nullptr)
        {
            _global_context = std::shared_ptr<Context>(new Context());
        }
        return _global_context;
    }

    void Context::reset_global_context()
    {
        _global_context = nullptr;
    }
}