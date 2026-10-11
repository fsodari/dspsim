#include <dspsim/context.h>
#include <dspsim/coro.h>
#include <dspsim/derived_signal.h>
#include <dspsim/event.h>
#include <dspsim/model.h>
#include <dspsim/module.h>
#include <dspsim/process.h>
#include <dspsim/signal.h>

#include "log.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace dspsim
{
    Context::Context(const std::string &name, int id)
        : name_(name),
          id_(id),
          next_model_id_(0),
          next_process_id_(0),
          //   active_process_(nullptr),
          time_(0),
          time_unit_("1ns"),
          update_count_(0)
    {
        if (name_.empty())
        {
            name_ = "context_" + std::to_string(id_);
        }
        // Logger names must be unique, and context names may repeat. The id makes it unique.
        logger_ = std::make_unique<detail::Logger>(name_ + "#" + std::to_string(id_));

        // Reserve space in eval/update/event stacks.
        process_eval_stack_.reserve(1000);
        signal_update_stack_.reserve(1000);
        sensitivity_event_stack_.reserve(1000);
    }

    Context::Context(Context &&) noexcept = default;

    Context::~Context()
    {
        clear();
    }

    void Context::clear()
    {
        // Drop everything that is scheduled, with drop() instead of clear(): FlaggedStack::clear()
        // resets the flag of each element, and signals may have gone out of scope already.
        process_eval_stack_.drop();
        signal_update_stack_.drop();
        derived_update_stack_.drop();
        current_process_ = nullptr;

        registered_models_.clear();
        modules_.clear();
        signals_.clear();
        processes_.clear();
        owned_models_.clear();
        children_.clear();

        sensitivity_event_stack_.clear();
        time_event_stack_ = PriorityQueue<TimeEvent>();
        trace_stack_.clear();
        active_module_stack_.clear();
        active_module_name_stack_.clear();
    }

    void Context::elaborate()
    {
        try
        {
            for (auto model : registered_models_)
            {
                model->finalize();
            }
        }
        catch (...)
        {
            release();
            throw;
        }
        elaborated_ = true;
        release();
    }

    bool Context::elaborated() const
    {
        return elaborated_;
    }

    void Context::release()
    {
        get_global_context_factory()->release(this);
    }

    bool Context::constructing() const
    {
        return get_global_context_factory()->is_active(this);
    }

    void Context::do_initialize()
    {
        if (initialized_) [[likely]]
            return;
        initialized_ = true;
        // Commit the initial signal values. Every process was scheduled when it was registered, unless
        // initialize(false) was called on it, so the first eval() settles the design from these values.
        update_signals();
    }

    void Context::update_signals()
    {
        while (!signal_update_stack_.empty())
        {
            SignalBase *signal = signal_update_stack_.back();
            signal_update_stack_.pop_back();
            DSPSIM_LOG_TRACE(logger_, "Updating signal: " + signal->name());
            signal->update();
        }

        // Derived signals are recomputed once all of their sources have committed.
        while (!derived_update_stack_.empty())
        {
            DerivedSignalBase *derived = derived_update_stack_.back();
            derived_update_stack_.pop_back();
            derived->pull();
        }
    }

    int Context::eval()
    {
        int n_iter = 0;
        bool any_model_updated = false;
        // Any model that was updated this cycle should be traced.

        // Run eval cycle.
        while (!process_eval_stack_.empty() || !signal_update_stack_.empty())
        {
            any_model_updated = !process_eval_stack_.empty();
            DSPSIM_LOG_TRACE(logger_, "Starting delta cycle iteration: " + std::to_string(n_iter));
            ++n_iter;

            // run eval cycle on all models that were scheduled to be evaluated.
            // A process is popped before it is resumed: a process that registers another process pushes
            // it onto the stack, and it is evaluated in this same delta cycle.
            while (!process_eval_stack_.empty())
            {
                ProcessBase *process = process_eval_stack_.back();
                process_eval_stack_.pop_back();
                DSPSIM_LOG_TRACE(logger_, "Evaluating process: " + process->name());
                current_process_ = process;
                process->mark_woken();
                process->resume();
            }
            if (any_model_updated)
            {
                // Increment the update count for this delta cycle.
                ++update_count_;
            }

            // run update cycle on all signals that were scheduled to be updated.
            update_signals();

            // Trigger all sensitivity events that were notified.
            for (const auto &event : sensitivity_event_stack_)
            {
                DSPSIM_LOG_TRACE(logger_, "Triggering sensitivity event");
                event->trigger();
            }
            sensitivity_event_stack_.clear();
        }

        // Trace modules that requested tracing.
        if (any_model_updated)
        {
            // Dump traces
            [[unlikely]] for (auto m : trace_stack_)
            {
                m->dump_trace();
            }
        }
        return n_iter;
    }

    void Context::run(uint64_t time_inc)
    {
        if (!initialized_) [[unlikely]]
        {
            do_initialize();
        }
        // Compute delta cycle. Signals may have been written to before the last run() call.
        // This will also eval any pending time updates from the last run() cycle.
        eval();

        // Evaluate all time steps.
        uint64_t next_time_step = 0;
        while (time_inc > 0)
        {
            // Advance to the next time step.
            if (!time_event_stack_.empty())
            {
                next_time_step = time_event_stack_.top().time_update() - time_;
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
            time_ += next_time_step;
            time_inc -= next_time_step;

            DSPSIM_LOG_TRACE(logger_, "Advancing simulation time by: " + std::to_string(next_time_step) + " to time: " + std::to_string(time_));

            // Queue all models for evaluation that have a zero time update.
            while (!time_event_stack_.empty() && time_event_stack_.top().time_update() == time_)
            {
                auto event = time_event_stack_.top();
                time_event_stack_.pop();
                DSPSIM_LOG_TRACE(logger_, "Popping time event subscriber: " + event.process()->name());
                schedule_time_event(event);
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

    void Context::schedule_time_event(const TimeEvent &event)
    {
        ProcessBase *process = event.process();
        if (event.sticky())
        {
            process_eval_stack_.push_back(process);
        }
        else if (event.wake_count() == process->wake_count())
        {
            // A coroutine time wait: the process stops waiting on anything else.
            process->time_wait_triggered();
            process_eval_stack_.push_back(process);
        }
        // Otherwise the process was resumed since the wait was scheduled, so the wakeup is stale.
    }

    void Context::schedule_time_delta_event(uint64_t time_delta)
    {
        if (current_process_->is_coroutine()) [[unlikely]]
        {
            throw std::logic_error("next_trigger(time) is not allowed in coroutine process " +
                                   current_process_->name() + ": use wait(time) instead");
        }
        time_event_stack_.emplace(time_ + time_delta, current_process_);
    }

    void Context::run_until_done(ProcessBase *process, uint64_t timeout)
    {
        if (!initialized_) [[unlikely]]
        {
            do_initialize();
        }
        const uint64_t deadline = time_ + timeout;
        eval();
        while (!process->done())
        {
            if (time_event_stack_.empty())
            {
                throw std::runtime_error("run_until: the simulation stalled at t=" + std::to_string(time_) +
                                         " with no pending time events, so '" + process->name() + "' can never complete.");
            }
            uint64_t next_time = time_event_stack_.top().time_update();
            if (timeout != 0 && next_time > deadline)
            {
                run(deadline - time_);
                throw TimeoutError("run_until: '" + process->name() + "' did not complete within " + std::to_string(timeout) +
                                   " time units (t=" + std::to_string(time_) + ").");
            }
            // Advance to the next time step and evaluate it.
            run(next_time - time_);
            eval();
        }
    }

    void Context::remove_process(ProcessBase *process)
    {
        process->unschedule();

        // Drop its pending time events. std::priority_queue has no erase, so rebuild it.
        std::vector<TimeEvent> kept;
        while (!time_event_stack_.empty())
        {
            if (time_event_stack_.top().process() != process)
            {
                kept.push_back(time_event_stack_.top());
            }
            time_event_stack_.pop();
        }
        time_event_stack_ = PriorityQueue<TimeEvent>(std::greater<TimeEvent>(), std::move(kept));

        process_eval_stack_.erase(process);
        if (current_process_ == process)
        {
            current_process_ = nullptr;
        }
        std::erase_if(processes_, [process](const std::unique_ptr<ProcessBase> &p)
                      { return p.get() == process; });
    }

    namespace
    {
        Task<bool> wait_event_task(Context *context, SensitivityEvent *event, uint64_t timeout)
        {
            if (timeout == 0)
            {
                co_await context->wait(*event);
                co_return true;
            }
            co_return (co_await context->wait(*event, timeout)) == WaitResult::Triggered;
        }
    } // namespace

    bool Context::run_until(SensitivityEvent &event, uint64_t timeout)
    {
        return run_until(wait_event_task(this, &event, timeout));
    }

    Wait Context::wait()
    {
        return Wait{current_process_};
    }

    WaitTimeEvent Context::wait(uint64_t time_delta)
    {
        return WaitTimeEvent{time_delta, current_process_};
    }

    WaitSensitivityEvent Context::wait(SensitivityEvent &event)
    {
        return WaitSensitivityEvent{event, current_process_};
    }
    WaitSensitivityEvent Context::wait(std::vector<std::reference_wrapper<SensitivityEvent>> events)
    {
        return WaitSensitivityEvent{events, current_process_};
    }

    WaitSensitivityEvent Context::wait(const std::vector<SensitivityEvent *> &events)
    {
        return WaitSensitivityEvent{events, current_process_};
    }

    WaitSensitivityEvent Context::wait(SensitivityEvent &event, uint64_t timeout)
    {
        return WaitSensitivityEvent{event, current_process_, timeout};
    }
    WaitSensitivityEvent Context::wait(std::vector<std::reference_wrapper<SensitivityEvent>> events, uint64_t timeout)
    {
        return WaitSensitivityEvent{events, current_process_, timeout};
    }
    WaitSensitivityEvent Context::wait(const std::vector<SensitivityEvent *> &events, uint64_t timeout)
    {
        return WaitSensitivityEvent{events, current_process_, timeout};
    }

    void Context::print_hierarchy(Model *parent, int depth) const
    {
        for (auto *child : children(parent))
        {
            DSPSIM_LOG_INFO(logger_, std::string(depth * 2, ' ') + child->name());
            print_hierarchy(child, depth + 1);
        }
    }
    const std::string &Context::name() const
    {
        return name_;
    }
    int Context::id() const
    {
        return id_;
    }

    const std::vector<Model *> &Context::models() const
    {
        return registered_models_;
    }

    const std::vector<Module *> &Context::modules() const
    {
        return modules_;
    }

    const std::vector<SignalBase *> &Context::signals() const
    {
        return signals_;
    }

    const std::vector<Model *> &Context::children(Model *parent) const
    {
        static const std::vector<Model *> empty;
        auto it = children_.find(parent);
        return it != children_.end() ? it->second : empty;
    }

    uint64_t Context::time() const
    {
        return time_;
    }

    const std::string &Context::time_unit() const
    {
        return time_unit_;
    }
    void Context::set_time_unit(const std::string &time_unit)
    {
        time_unit_ = time_unit;
    }

    const std::string Context::log_level() const
    {
        return logger_->level();
    }
    void Context::set_log_level(const std::string &log_level)
    {
        logger_->set_level(log_level);
    }

    const std::string Context::repr() const
    {
        return "Context(id=" + std::to_string(id_) + ", time=" + std::to_string(time_) + ")";
    }

    void Context::log(const std::string &level, const std::string &message)
    {
        logger_->log(level, message);
    }

    void Context::add_model(Model *model)
    {
        model->hier_name_ = current_hierarchy() + "." + model->name();
        DSPSIM_LOG_TRACE(logger_, "Adding model: " + model->name() + ", hier: " + current_hierarchy());
        registered_models_.push_back(model);
        children_[model->parent()].push_back(model);

        // Reset the active process of the context.
        // Processes must be registered after all other submodules have been added to a parent module.
        // active_process_ = nullptr;
    }
    void Context::add_module(Module *module)
    {
        modules_.push_back(module);
    }

    void Context::add_signal(SignalBase *signal)
    {
        signals_.push_back(signal);
    }

    const std::vector<std::shared_ptr<Model>> &Context::owned_models() const
    {
        return owned_models_;
    }

    uint32_t Context::next_model_id()
    {
        return next_model_id_++;
    }

    uint32_t Context::next_process_id()
    {
        return next_process_id_++;
    }

    void Context::trace_model(Model *model)
    {
        trace_stack_.push_back(model);
    }

    void Context::own_model(std::shared_ptr<Model> model)
    {
        owned_models_.push_back(model);
    }

    ProcessBase *Context::add_process(std::unique_ptr<ProcessBase> process)
    {
        processes_.push_back(std::move(process));
        ProcessBase *p = processes_.back().get();
        process_eval_stack_.push_back(p);
        return p;
    }

    ProcessBase *Context::register_process_func(const std::function<void()> &eval, const std::string &name)
    {
        DSPSIM_LOG_INFO(logger_, "Registering process: " + name);
        return add_process(std::make_unique<Process>(this, eval, name));
    }

    ProcessBase *Context::register_trampoline(void *instance, void (*call)(void *), const std::string &name)
    {
        DSPSIM_LOG_INFO(logger_, "Registering process: " + name);
        return add_process(std::make_unique<MethodProcess>(this, instance, call, name));
    }

    ProcessBase *Context::register_coro(std::coroutine_handle<> root, const std::string &name)
    {
        DSPSIM_LOG_INFO(logger_, "Registering coroutine task: " + name);
        return add_process(std::make_unique<CoroProcess>(this, root, name));
    }

    Module *Context::active_module() const
    {
        return active_module_stack_.empty() ? nullptr : active_module_stack_.back();
    }

    const std::string Context::current_hierarchy() const
    {
        std::string hierarchy = "root";
        for (auto model : active_module_stack_)
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
    struct ContextFactory::State
    {
        int next_context_id = 0;
        std::shared_ptr<Context> active_context;
        // The thread constructing the active context.
        std::thread::id owner;
        // Guards the active context and its owner. Never held while calling into models or contexts.
        std::mutex mutex;
        // Notified when the active context is released.
        std::condition_variable released;
    };

    ContextFactory::ContextFactory()
        : state_(std::make_unique<State>())
    {
    }

    ContextFactory::~ContextFactory() = default;

    std::shared_ptr<Context> ContextFactory::obtain()
    {
        std::lock_guard lock(state_->mutex);
        if (state_->active_context == nullptr || state_->owner != std::this_thread::get_id())
        {
            throw ContextConstructionError(
                "There is no active context in this thread. Call Context::create() before constructing models. "
                "A context is no longer active once it has been elaborated or released.");
        }
        return state_->active_context;
    }

    void ContextFactory::reset()
    {
        release_active([](const Context *)
                       { return true; });
    }

    void ContextFactory::release(const Context *context)
    {
        release_active([context](const Context *active)
                       { return active == context; });
    }

    bool ContextFactory::is_active(const Context *context)
    {
        std::lock_guard lock(state_->mutex);
        return state_->active_context.get() == context;
    }

    std::shared_ptr<Context> ContextFactory::create(const std::string &name)
    {
        std::unique_lock lock(state_->mutex);
        // Waiting on a context this thread is constructing would deadlock.
        if (state_->active_context && state_->owner == std::this_thread::get_id())
        {
            throw ContextConstructionError(
                "Cannot create context '" + name + "': context '" + state_->active_context->name() +
                "' is still under construction in this thread. "
                "Call elaborate() or release() on it first, or Context::reset() to discard it.");
        }
        state_->released.wait(lock, [this]
                              { return state_->active_context == nullptr; });
        state_->active_context = std::shared_ptr<Context>(new Context(name, state_->next_context_id++));
        state_->owner = std::this_thread::get_id();
        return state_->active_context;
    }

    void ContextFactory::release_active(const std::function<bool(const Context *)> &if_active)
    {
        // The context may be destroyed when released. Do that outside of the lock.
        std::shared_ptr<Context> released;
        {
            std::lock_guard lock(state_->mutex);
            if (!if_active(state_->active_context.get()))
            {
                return;
            }
            released = std::move(state_->active_context);
            state_->owner = {};
        }
        state_->released.notify_all();
    }

    static std::shared_ptr<ContextFactory> global_context_factory = nullptr;

    // Guards global_context_factory. Threads may create their first context concurrently, and must all get the same factory.
    // A function-local static, so it is usable from other translation units' static initializers.
    static std::mutex &global_context_factory_mutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    std::shared_ptr<ContextFactory> get_global_context_factory()
    {
        std::lock_guard lock(global_context_factory_mutex());
        if (!global_context_factory)
        {
            global_context_factory = std::make_shared<ContextFactory>();
        }
        return global_context_factory;
    }

    void set_global_context_factory(std::shared_ptr<ContextFactory> factory)
    {
        std::lock_guard lock(global_context_factory_mutex());
        global_context_factory = factory;
    }

    void reset_global_context_factory()
    {
        // The factory may be destroyed here. Do that outside of the lock.
        std::shared_ptr<ContextFactory> factory;
        std::lock_guard lock(global_context_factory_mutex());
        factory = std::move(global_context_factory);
        global_context_factory = nullptr;
    }
}
