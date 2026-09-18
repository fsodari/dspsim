#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include "timestrings.h"
#include <format>
#include <iostream>
#include <algorithm>
#include <dspsim/module_name.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
namespace dspsim
{
    // // Global context pointer. Models will self-register with the global context.
    // static ContextPtr _global_context = nullptr;

    // // Initialize each new context with a new id.
    // static int next_context_id = 0;

    Context::Context(int id)
        : _id(id),
          _next_model_id(0),
          _time(0),
          _time_unit("1ns")
    {
        this->logger = spdlog::stdout_color_mt("context");
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%^%l%$] %v");
    }

    Context::~Context()
    {
        spdlog::drop("context");
        clear();
    }

    void Context::add_model(Model *model)
    {
        model->_id = _next_model_id++;
        model->_hier_name = hier() + "." + model->name();
        SPDLOG_LOGGER_TRACE(logger, "Adding model: {}, hier: {}", model->name(), hier());
        _registered_models.push_back(model);
    }
    const std::string Context::hier() const
    {
        std::string hierarchy = "root";
        for (auto model : _active_module_stack)
        {
            if (!hierarchy.empty())
                hierarchy += ".";
            hierarchy += model->name();
        }
        return hierarchy;
    }

    void Context::own_model(ModelPtr model)
    {
        _owned_models.push_back(model);
    }

    void Context::clear()
    {
        _registered_models.clear();
        _owned_models.clear();
    }

    void Context::elaborate()
    {
        for (auto model : _registered_models)
        {
            model->finalize();
        }
    }

    void Context::add_to_time_event_queue(TimeEvent event)
    {
        _time_event_queue.push_back(event);
    }

    TimeEvent Context::pop_from_time_event_queue()
    {
        auto event = _time_event_queue.front();
        _time_event_queue.pop_front();
        return event;
    }

    void Context::eval()
    {
    }

    int Context::delta_cycle()
    {
        int n_iter = 0;

        // Run eval cycle.
        do
        {
            // std::vector<Model *> sync_queue;
            SPDLOG_LOGGER_TRACE(logger, "Starting delta cycle iteration: {}", n_iter);
            ++n_iter;
            int n_evals = 0;
            int n_syncs = 0;
            while (!_eval_queue.empty())
            {
                auto *model = pop_from_eval_queue();
                SPDLOG_LOGGER_TRACE(logger, "Evaluating model: {}", model->name());
                // sync_queue.push_back(model);
                model->eval();
                ++n_evals;
            }
            while (!_update_queue.empty())
            {
                auto *model = pop_from_update_queue();
                SPDLOG_LOGGER_TRACE(logger, "Updating model: {}", model->name());
                model->update();
                ++n_syncs;
            }
        } while (!_eval_queue.empty());

        return n_iter;
    }

    void Context::run(uint64_t time_inc)
    {
        uint64_t remaining_time = time_inc;
        // uint64_t next_time_step = _time_event_queue.empty() ? _time + remaining_time : _time_event_queue.begin()->time();
        // Compute delta cycle.
        delta_cycle();

        // Sort the time event queue

        while (!_time_event_queue.empty() and remaining_time > 0)
        {
            std::sort(_time_event_queue.begin(), _time_event_queue.end());
            // Advance to the next time step.
            uint64_t next_time_step = _time_event_queue.front().time_update - _time;
            // Advance the simulation time to the next time step.
            _time += next_time_step;
            SPDLOG_LOGGER_TRACE(logger, "Advancing simulation time by: {} to time: {}", next_time_step, _time);

            // Queue all models for evaluation that have a zero time update.
            do
            {
                auto event = pop_from_time_event_queue();
                SPDLOG_LOGGER_TRACE(logger, "Popping time event subscriber: {}", event.subscriber->name());
                add_to_eval_queue(event.subscriber);
            } while (!_time_event_queue.empty() && _time_event_queue.front().time_update == _time);
            // Perform a delta cycle.
            delta_cycle();
            remaining_time -= next_time_step;
        }
    }

    void Context::add_to_eval_queue(Model *model)
    {
        _eval_queue.insert(model);
    }
    Model *Context::pop_from_eval_queue()
    {
        auto it = _eval_queue.begin();
        Model *model = *it;
        _eval_queue.erase(it);
        return model;
    }

    void Context::add_to_update_queue(Model *model)
    {
        _update_queue.insert(model);
    }

    Model *Context::pop_from_update_queue()
    {
        auto it = _update_queue.begin();
        Model *model = *it;
        _update_queue.erase(it);
        return model;
    }

    const std::string Context::repr() const
    {
        return std::format("Context(id={}, time={})", _id, _time);
    }

    ContextPtr Context::obtain()
    {
        return get_global_context_factory()->obtain();
    }

    void Context::reset_global_context()
    {
        get_global_context_factory()->reset();
    }

    // Context Factory
    ContextFactory::ContextFactory()
        : _next_context_id(0),
          _active_context(nullptr)
    {
    }

    ContextPtr ContextFactory::obtain()
    {
        if (_active_context == nullptr)
        {
            _active_context = std::shared_ptr<Context>(new Context(_next_context_id++));
        }
        return _active_context;
    }

    void ContextFactory::reset()
    {
        _active_context = nullptr;
    }

    static ContextFactoryPtr _global_context_factory = nullptr;

    ContextFactoryPtr get_global_context_factory()
    {
        if (!_global_context_factory)
        {
            _global_context_factory = std::make_shared<ContextFactory>();
        }
        return _global_context_factory;
    }

    void set_global_context_factory(ContextFactoryPtr factory)
    {
        _global_context_factory = factory;
    }
    void reset_global_context_factory()
    {
        _global_context_factory = nullptr;
    }
}