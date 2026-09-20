#include <dspsim/context.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <dspsim/event.h>
#include <dspsim/signal.h>

#include "timestrings.h"
#include <format>
#include <iostream>
#include <algorithm>
#include <dspsim/module_name.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
namespace dspsim
{
    static inline auto parse_level(const std::string &level)
    {
        // Convert to lowercase
        std::string lower_log_level = level;
        std::transform(lower_log_level.begin(), lower_log_level.end(), lower_log_level.begin(), ::tolower);
        spdlog::level::level_enum parsed_level = spdlog::level::from_str(lower_log_level);
        if (parsed_level == spdlog::level::off && lower_log_level != "off")
        {
            // Invalid string level, use info as fallback.
            return spdlog::level::info;
        }
        else
        {
            return parsed_level;
        }
    }

    Context::Context(const std::string &name, int id)
        : _name(name),
          _id(id),
          _next_model_id(0),
          _time(0),
          _time_unit("1ns"),
          _signal_event(false)
    {
        this->logger = spdlog::stdout_color_mt(_name);
        logger->set_level(spdlog::level::warn);
        logger->set_pattern("[%^%l%$] %v");
    }

    Context::~Context()
    {
        spdlog::drop(_name);
        clear();
    }

    void Context::clear()
    {
        _registered_models.clear();
        _owned_models.clear();
        _children.clear();
        _eval_stack.clear();
        _update_stack.clear();
        _time_event_stack.clear();
    }

    void Context::elaborate()
    {
        for (auto model : _registered_models)
        {
            model->finalize();
        }

        // Force an initial settle: schedule every module for evaluation once so that
        // combinational logic propagates from initial signal values before the first eval().
        for (auto model : _registered_models)
        {
            if (auto *module = dynamic_cast<Module *>(model))
            {
                if (module->initialize())
                {
                    _push_eval_stack(model);
                }
            }
        }
    }

    int Context::eval()
    {
        int n_iter = 0;

        // Any model that was updated this cycle should be traced.
        UniqueStack<Model *> _trace_stack;

        // Reset signal event flag at the beginning of each delta cycle.
        if (_signal_event)
        {
            _signal_event = false;
            // Reset state of all signals.
            for (auto signal : _signals)
            {
                signal->_clear_event_flag();
            }
        }
        // Run eval cycle.
        while (!_eval_stack.empty())
        {
            SPDLOG_LOGGER_TRACE(logger, "Starting delta cycle iteration: {}", n_iter);
            ++n_iter;

            while (!_eval_stack.empty())
            {
                // auto model = _eval_stack.pop();
                auto model = _eval_stack.back();
                _eval_stack.pop_back();
                SPDLOG_LOGGER_TRACE(logger, "Evaluating model: {}", model->name());
                model->eval();

                // Add the model to the update stack after evaluation.
                _update_stack.push_back(model);
            }

            // run update cycle on all models that were evaluated.
            while (!_update_stack.empty())
            {
                auto model = _update_stack.back();
                _update_stack.pop_back();
                SPDLOG_LOGGER_TRACE(logger, "Updating model: {}", model->name());
                model->update();

                _trace_stack.push_back(model);
            }
        }

        // Trace.
        for (auto m : _trace_stack)
        {
            m->dump_trace();
        }
        return n_iter;
    }

    void Context::run(uint64_t time_inc)
    {
        // Compute delta cycle.
        eval();

        // Evaluate all time steps.
        while (!_time_event_stack.empty() and time_inc > 0)
        {
            // Advance to the next time step.
            uint64_t next_time_step = _time_event_stack.top().time_update - _time;
            // Advance the simulation time to the next time step.
            _time += next_time_step;
            time_inc -= next_time_step;
            SPDLOG_LOGGER_TRACE(logger, "Advancing simulation time by: {} to time: {}", next_time_step, _time);

            // Queue all models for evaluation that have a zero time update.
            do
            {
                auto event = _time_event_stack.pop();
                SPDLOG_LOGGER_TRACE(logger, "Popping time event subscriber: {}", event.subscriber->name());
                // push_eval_stack(event.subscriber);
                _eval_stack.push_back(event.subscriber);
            } while (!_time_event_stack.empty() && _time_event_stack.top().time_update == _time);
            // Perform a delta cycle at this time step.
            eval();
        }
    }

    void Context::print_hierarchy(Model *parent, int depth) const
    {
        for (auto *child : children(parent))
        {
            logger->info("{}{}", std::string(depth * 2, ' '), child->name());
            print_hierarchy(child, depth + 1);
        }
    }
    const std::string &Context::name() const
    {
        return _name;
    }
    int Context::id() const
    {
        return _id;
    }

    const std::vector<Model *> &Context::models() const
    {
        return _registered_models;
    }

    const std::vector<Module *> &Context::modules() const
    {
        return _modules;
    }

    const std::vector<Model *> &Context::children(Model *parent) const
    {
        static const std::vector<Model *> empty;
        auto it = _children.find(parent);
        return it != _children.end() ? it->second : empty;
    }

    uint64_t Context::time() const
    {
        return _time;
    }

    const std::string &Context::time_unit() const
    {
        return _time_unit;
    }
    void Context::set_time_unit(const std::string &time_unit)
    {
        _time_unit = time_unit;
    }

    const std::string Context::log_level() const
    {
        auto view = spdlog::level::to_string_view(logger->level());
        return std::string{view.begin(), view.end()};
    }
    void Context::set_log_level(const std::string &log_level)
    {
        auto level = parse_level(log_level);
        logger->set_level(level);
    }

    const std::string Context::repr() const
    {
        return std::format("Context(id={}, time={})", _id, _time);
    }

    void Context::log(const std::string &level, const std::string &message)
    {
        auto lvl = parse_level(level);
        logger->log(lvl, message);
    }

    void Context::_add_model(Model *model)
    {
        model->_id = _next_model_id++;
        model->_hier_name = _current_hierarchy() + "." + model->name();
        SPDLOG_LOGGER_TRACE(logger, "Adding model: {}, hier: {}", model->name(), _current_hierarchy());
        _registered_models.push_back(model);
        _children[model->parent()].push_back(model);

        // If this is a module, add it to the list of modules.
        if (auto *module = dynamic_cast<Module *>(model))
        {
            _modules.push_back(module);
        }
    }

    void Context::_add_signal(SignalBase *signal)
    {
        _signals.push_back(signal);
    }

    void Context::_own_model(ModelPtr model)
    {
        _owned_models.push_back(model);
    }

    void Context::_own_module(ModulePtr module)
    {
        _owned_modules.push_back(module);
    }

    void Context::_push_eval_stack(Model *model)
    {
        _eval_stack.push_back(model);
    }

    void Context::_push_time_event_stack(TimeEvent event)
    {
        _time_event_stack.push(event);
    }

    Module *Context::_active_module() const
    {
        return _active_module_stack.empty() ? nullptr : _active_module_stack.back();
    }

    const std::string Context::_current_hierarchy() const
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

    ContextPtr Context::obtain()
    {
        return get_global_context_factory()->obtain();
    }

    void Context::reset()
    {
        get_global_context_factory()->reset();
    }
    ContextPtr Context::create(const std::string &name)
    {
        return get_global_context_factory()->create(name);
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
            // If there is no active context, create a new one with the next available context ID.
            // Give an empty name.
            _active_context = std::shared_ptr<Context>(new Context("", _next_context_id++));
        }
        return _active_context;
    }

    void ContextFactory::reset()
    {
        _active_context = nullptr;
    }

    ContextPtr ContextFactory::create(const std::string &name)
    {
        _active_context = nullptr;
        std::string _context_name = name;
        if (name.empty())
        {
            _context_name = "context_" + std::to_string(_next_context_id);
        }
        _active_context = std::shared_ptr<Context>(new Context(_context_name, _next_context_id++));
        return _active_context;
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