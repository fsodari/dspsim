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
          _next_process_id(0),
          //   _active_process(nullptr),
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
        // Do not touch spdlog's global registry here: Context can be destroyed by the
        // static destruction of _global_context_factory, and spdlog's registry (a
        // function-local static) may already have been torn down by that point,
        // making spdlog::drop() dereference freed memory.
        clear();
    }

    void Context::clear()
    {
        // Clearing this will cause a segfault if signals go out of scope first.
        // _process_eval_stack.clear();
        // _signal_update_stack.clear();
        _registered_models.clear();
        _owned_models.clear();
        _children.clear();
        _time_event_stack = PriorityQueue<TimeEvent>();
    }

    void Context::elaborate()
    {
        for (auto model : _registered_models)
        {
            model->finalize();
        }

        // Force an initial settle: schedule every module for evaluation once so that
        // combinational logic propagates from initial signal values before the first eval().
        for (auto process : _processes)
        {
            // Modules can opt out of initializing.
            if (process->source() == nullptr)
                continue;
            if (auto *module = dynamic_cast<Module *>(process->source()))
            {
                if (module->initialize())
                {
                    _process_eval_stack.push_back(process.get());
                }
            }
            else
            {
                _process_eval_stack.push_back(process.get());
            }
        }

        // Force an initial update of all signals.
        for (auto signal : _signals)
        {
            _signal_update_stack.push_back(signal);
        }
    }

    int Context::eval()
    {
        int n_iter = 0;
        bool any_model_updated = false;
        // Any model that was updated this cycle should be traced.

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
        // while (!_eval_stack.empty() || !_process_eval_stack.empty() || !_signal_update_stack.empty())
        while (!_process_eval_stack.empty() || !_signal_update_stack.empty())
        {
            any_model_updated = true;
            SPDLOG_LOGGER_TRACE(logger, "Starting delta cycle iteration: {}", n_iter);
            ++n_iter;

            // run update cycle on all models that were evaluated.
            while (!_process_eval_stack.empty())
            {
                Process *process = _process_eval_stack.back();
                _process_eval_stack.pop_back();
                SPDLOG_LOGGER_TRACE(logger, "Evaluating process: {}", process->name());
                process->eval();
            }

            while (!_signal_update_stack.empty())
            {
                SignalBase *signal = _signal_update_stack.back();
                _signal_update_stack.pop_back();
                SPDLOG_LOGGER_TRACE(logger, "Updating signal: {}", signal->name());
                signal->update();
            }
        }

        // Trace modules that requested tracing.
        if (any_model_updated)
        {
            [[unlikely]] for (auto m : _trace_stack)
            {
                m->dump_trace();
            }
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
            // What if time update is less than the current time? If we missed a step? Bad model.

            // If the next time step exceeds the remaining time increment, limit it to the remaining time increment.
            if (next_time_step > time_inc)
            {
                next_time_step = time_inc;
            }
            // Advance the simulation time to the next time step.
            _time += next_time_step;
            time_inc -= next_time_step;
            SPDLOG_LOGGER_TRACE(logger, "Advancing simulation time by: {} to time: {}", next_time_step, _time);

            // Queue all models for evaluation that have a zero time update.
            while (!_time_event_stack.empty() && _time_event_stack.top().time_update == _time)
            {
                auto event = _time_event_stack.top();
                _time_event_stack.pop();
                SPDLOG_LOGGER_TRACE(logger, "Popping time event subscriber: {}", event.process->name());
                _process_eval_stack.push_back(event.process);
            }
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

    const std::vector<SignalBase *> &Context::signals() const
    {
        return _signals;
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

        // Reset the active process of the context.
        // Processes must be registered after all other submodules have been added to a parent module.
        // _active_process = nullptr;
    }
    void Context::_add_module(Module *module)
    {
        _modules.push_back(module);
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

    Process *Context::register_process_func(const std::function<void()> &eval, Model *source, const std::string &name)
    {
        auto process = std::make_shared<Process>(_next_process_id++, eval, source, name);
        _processes.push_back(process);
        logger->info("Registering process: {}, id: {}", name, _next_process_id - 1);
        // Set the active process of the context.
        // _active_process = process.get();
        return process.get();
    }

    Module *Context::_active_module() const
    {
        return _active_module_stack.empty() ? nullptr : _active_module_stack.top();
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