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
        if (_name.empty())
        {
            _name = "context_" + std::to_string(_id);
        }
        this->logger = spdlog::stdout_color_mt(_name);
        logger->set_level(spdlog::level::warn);
        logger->set_pattern("[%^%l%$] %v");

        // Reserve space in eval/update/event stacks.
        _process_eval_stack.stack().reserve(1000);
        _signal_update_stack.stack().reserve(1000);
        _sensitivity_event_stack.reserve(1000);
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
        // Clearing these will cause a segfault if signals go out of scope first.
        // _process_eval_stack.clear();
        // _signal_update_stack.clear();

        _registered_models.clear();
        _modules.clear();
        _signals.clear();
        _processes.clear();
        _owned_models.clear();
        _children.clear();

        _sensitivity_event_stack.clear();
        _time_event_stack = PriorityQueue<TimeEvent>();
        _trace_stack.clear();
        _active_module_stack.clear();
        _active_module_name_stack.clear();
    }

    void Context::elaborate()
    {
        for (auto model : _registered_models)
        {
            model->finalize();
        }
    }

    void Context::_do_initialize()
    {
        if (_initialized) [[likely]]
            return;
        _initialized = true;
        // Update all signals
        while (!_signal_update_stack.empty())
        {
            SignalBase *signal = _signal_update_stack.back();
            _signal_update_stack.pop_back();
            SPDLOG_LOGGER_TRACE(logger, "Updating signal: {}", signal->name());
            signal->update();
        }

        // Force an initial settle: schedule every module for evaluation once so that
        // combinational logic propagates from initial signal values before the first eval().
        for (auto &process : _processes)
        {
            logger->debug("Processing initialization for process: {}, init={}", process->name(), process->initialize());
            if (process->initialize())
            {
                _process_eval_stack.push_back(process.get());
            }
        }
    }

    int Context::eval()
    {
        int n_iter = 0;
        bool any_model_updated = false;
        // Any model that was updated this cycle should be traced.

        // Reset signal event flag at the beginning of each delta cycle.
        // How can I clear this state without needing to iterate through every signal.
        // Is this in the right place?
        if (_signal_event)
        {
            _signal_event = false;
            // Reset state of all signals.
            for (auto signal : _signals)
            {
                signal->clear_event_flag();
            }
        }

        // Run eval cycle.
        while (!_process_eval_stack.empty() || !_signal_update_stack.empty())
        {
            any_model_updated = true;
            SPDLOG_LOGGER_TRACE(logger, "Starting delta cycle iteration: {}", n_iter);
            ++n_iter;

            // run eval cycle on all models that were scheduled to be evaluated.
            for (const auto &process : _process_eval_stack)
            {
                SPDLOG_LOGGER_TRACE(logger, "Evaluating process: {}", process->name());
                _current_process = process;
                process->resume();
            }
            _process_eval_stack.clear();

            // run update cycle on all signals that were scheduled to be updated.
            for (const auto &signal : _signal_update_stack)
            {
                SPDLOG_LOGGER_TRACE(logger, "Updating signal: {}", signal->name());
                signal->update();
            }
            _signal_update_stack.clear();

            // run notify cycle on all sensitivity events that were scheduled to be notified.
            for (const auto &event : _sensitivity_event_stack)
            {
                SPDLOG_LOGGER_TRACE(logger, "Notifying sensitivity event");
                event->notify();
            }
            _sensitivity_event_stack.clear();
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
        if (!_initialized) [[unlikely]]
        {
            _do_initialize();
        }
        // Compute delta cycle. Signals may have been written to before the last run() call.
        // This will also eval any pending time updates from the last run() cycle.
        eval();

        // Evaluate all time steps.
        uint64_t next_time_step = 0;
        while (time_inc > 0)
        {
            // Advance to the next time step.
            if (!_time_event_stack.empty())
            {
                next_time_step = _time_event_stack.top().time_update() - _time;
                // If the next time step exceeds the remaining time increment, limit it to the remaining time increment.
                if (next_time_step > time_inc)
                {
                    next_time_step = time_inc;
                }
            }
            else
            {
                next_time_step = time_inc;
            }
            // What if time update is less than the current time? If we missed a step? Bad model.

            // Advance the simulation time to the next time step.
            _time += next_time_step;
            time_inc -= next_time_step;

            SPDLOG_LOGGER_TRACE(logger, "Advancing simulation time by: {} to time: {}", next_time_step, _time);

            // Queue all models for evaluation that have a zero time update.
            while (!_time_event_stack.empty() && _time_event_stack.top().time_update() == _time)
            {
                auto event = _time_event_stack.top();
                _time_event_stack.pop();
                SPDLOG_LOGGER_TRACE(logger, "Popping time event subscriber: {}", event.process()->name());
                _process_eval_stack.push_back(event.process());
            }
            // Evaluate up until the next time step. So we should skip an eval when time_inc == 0.
            // Models with the time update will still be queued for the next delta cycle.
            if (time_inc != 0)
            {
                // Perform a delta cycle at this time step.
                eval();
            }
        }
    }

    void Context::schedule_time_delta_event(uint64_t time_delta, ProcessBase *process)
    {
        if (process == nullptr)
        {
            process = _current_process;
        }
        _time_event_stack.emplace(_time + time_delta, process);
    }

    WaitTimeEvent Context::wait(uint64_t time_delta, ProcessBase *process)
    {
        if (process == nullptr)
        {
            process = _current_process;
        }
        return WaitTimeEvent{time_delta, process};
    }

    WaitSensitivityEvent Context::wait(ProcessBase *process)
    {
        if (process == nullptr)
        {
            process = _current_process;
        }
        return WaitSensitivityEvent{process};
    }

    WaitSensitivityEvent Context::wait(SensitivityEvent &event, ProcessBase *process)
    {
        if (process == nullptr)
        {
            process = _current_process;
        }
        return WaitSensitivityEvent{event, process};
    }
    WaitSensitivityEvent Context::wait(std::initializer_list<std::reference_wrapper<SensitivityEvent>> events, ProcessBase *process)
    {
        if (process == nullptr)
        {
            process = _current_process;
        }
        return WaitSensitivityEvent{events, process};
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

    void Context::_own_model(std::shared_ptr<Model> model)
    {
        _owned_models.push_back(model);
    }

    ProcessBase *Context::register_process_func(const std::function<void()> &eval, const std::string &name)
    {
        _processes.emplace_back(std::make_unique<Process>(eval, name));
        logger->info("Registering process: {}", name);
        return _processes.back().get();
    }

    ProcessBase *Context::register_coro_task(Task task, const std::string &name)
    {
        _processes.emplace_back(std::make_unique<CoroProcess>(std::move(task), name));
        logger->info("Registering coroutine task: {}", name);
        return _processes.back().get();
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

    std::shared_ptr<Context> Context::obtain()
    {
        return get_global_context_factory()->obtain();
    }

    void Context::reset()
    {
        get_global_context_factory()->reset();
    }
    std::shared_ptr<Context> Context::create(const std::string &name)
    {
        return get_global_context_factory()->create(name);
    }

    // Context Factory
    ContextFactory::ContextFactory()
        : _next_context_id(0),
          _active_context(nullptr)
    {
    }

    std::shared_ptr<Context> ContextFactory::obtain()
    {
        if (_active_context == nullptr)
        {
            // If there is no active context, create one with default name.
            _active_context = std::shared_ptr<Context>(new Context("", _next_context_id++));
        }
        return _active_context;
    }

    void ContextFactory::reset()
    {
        _active_context = nullptr;
    }

    std::shared_ptr<Context> ContextFactory::create(const std::string &name)
    {
        _active_context = nullptr;
        _active_context = std::shared_ptr<Context>(new Context(name, _next_context_id++));
        return _active_context;
    }

    static std::shared_ptr<ContextFactory> _global_context_factory = nullptr;

    std::shared_ptr<ContextFactory> get_global_context_factory()
    {
        if (!_global_context_factory)
        {
            _global_context_factory = std::make_shared<ContextFactory>();
        }
        return _global_context_factory;
    }

    void set_global_context_factory(std::shared_ptr<ContextFactory> factory)
    {
        _global_context_factory = factory;
    }
    void reset_global_context_factory()
    {
        _global_context_factory = nullptr;
    }
}