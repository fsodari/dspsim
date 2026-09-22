#pragma once
#include <dspsim/forward.h>
#include <dspsim/model.h>
#include <dspsim/signal.h>
#include <dspsim/event.h>
#include <dspsim/process.h>
#include <dspsim/utils/unique_stack.h>
#include <dspsim/utils/priority_queue.h>
#include <memory>
#include <vector>
#include <string>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <functional>
#include <mutex>
#include <atomic>

namespace spdlog
{
    class logger;
}

namespace dspsim
{

    /*
        Context contains a vector of all the models.
        Responsible for elaboration, simulation, and management of models.

        Context states:
        - Design setup. Models can be added to the context. Models will self-register with the global context, so this
          must be locked to prevent other threads from adding models to the context while it is being set up.
        - Detached. Once elaboration is complete, the context is detached from the global context.
          The context can be used for simulation, but no new models can be added to it.
          New contexts can be created with new parameters so that they can run in parallel.
    */
    using ContextPtr = std::shared_ptr<class Context>;
    class Context
    {
        friend class ContextFactory;
        // Members
    private:
        // Context info
        std::string _name;
        int _id;
        // Each model is assigned a unique ID, starting from 0.
        uint32_t _next_model_id;
        // List of all registered models, modules, and signals.
        std::vector<Model *> _registered_models;
        std::vector<Module *> _modules;
        std::vector<SignalBase *> _signals;

        // Each process is assigned a unique ID, starting from 0.
        uint32_t _next_process_id;
        std::vector<std::shared_ptr<Process>> _processes;

        // Owned models stay alive with context.Necessary if a design is created in a function and the context is returned.
        // More likely to be used in Python
        std::vector<ModelPtr> _owned_models;
        std::vector<ModulePtr> _owned_modules;

        // Design hierarchy: maps a model to its direct children (root models are keyed by nullptr).
        std::unordered_map<Model *, std::vector<Model *>> _children;

    public:
        // All processes that need to run in the current delta cycle.
        UniqueStack<Process *> _process_eval_stack;
        // All signals that need to be updated in the current delta cycle.
        UniqueStack<SignalBase *> _signal_update_stack;
        // The last declared process. Used with sensitivity lists.
        Process *_active_process;

    private:
        // Guards _signal_update_stack, since processes may run concurrently on the TMC thread
        // pool and each call Signal::write(), which schedules/unschedules signal updates.
        std::mutex _signal_update_mutex;
        // Set only while a batch is actually dispatched to the TMC thread pool. Lets
        // _schedule_signal_update/_unschedule_signal_update skip locking entirely for the common
        // single-threaded round, which has no concurrent writers.
        std::atomic<bool> _process_eval_parallel{false};

        // Reused scratch buffer for draining _process_eval_stack each round, to avoid a heap
        // allocation per delta cycle.
        std::vector<Process *> _process_eval_batch;
        // Below this many pending processes, the TMC thread pool's dispatch/wake latency
        // outweighs any parallelism benefit, so the round is run inline instead.
        size_t _parallel_eval_threshold = 64;

        // Current simulation time.
        uint64_t _time;
        // Time unit used for tracing.
        std::string _time_unit;

        /*
        Pseudo-private members that are not intended
        to be accessed publicly. Need to set up friend classes.
        */
    public:
        std::shared_ptr<spdlog::logger> logger;

        // Keep track of the currently active module to build a hierarchy.
        std::deque<Module *> _active_module_stack;
        // ModuleName is used as a way of building the module hierarchy and running cleanup when Module construction ends.
        std::deque<ModuleName *> _active_module_name_stack;
        // Scheduled time events.
        PriorityQueue<TimeEvent> _time_event_stack;

        // Flag indicating if there has been a signal event in the current delta cycle.
        // All signal event flags are cleared at the start of every delta cycle.
        bool _signal_event;

    private:
        // Can't create context directly. Must use obtain() to get global context, or create() to make a new global context.
        Context(const std::string &name, int id);

    public:
        ~Context();

        // Methods
        /*
            Clear all models from the context.
        */
        void clear();

        /*
            Call elaborate after construction is complete.
            This will finalize all port bindings,
            and TODO: check for any netlist violations.
        */
        void elaborate();

        // Compute a single delta cycle.
        int eval();

        /*
            Run the simulation for the given time increment.
            If time_inc is 0, it will run a delta cycle without advancing time.
        */
        void run(uint64_t time_inc = 0);

        // Log the model hierarchy, starting from the given parent (nullptr = roots).
        void print_hierarchy(Model *parent = nullptr, int depth = 0) const;

        // Minimum pending processes in a delta round before dispatching to the TMC thread pool
        // instead of running inline. Mainly useful for benchmarking the crossover point.
        size_t parallel_eval_threshold() const { return _parallel_eval_threshold; }
        void set_parallel_eval_threshold(size_t threshold) { _parallel_eval_threshold = threshold; }

        // Properties
        // Context name. Initialized when created.
        const std::string &name() const;

        // Context id. Initialized when created.
        int id() const;

        // List of all registered models in the context.
        const std::vector<Model *> &models() const;

        // List of all registered modules in the context.
        const std::vector<Module *> &modules() const;

        // List of all registered signals in the context.
        const std::vector<SignalBase *> &signals() const;

        // Direct children of a model in the design hierarchy. Pass nullptr for the top-level (root) models.
        const std::vector<Model *> &children(Model *parent = nullptr) const;

        // Current simulation time.
        uint64_t time() const;

        // Time unit. Necessary for tracing.
        const std::string &time_unit() const;
        void set_time_unit(const std::string &time_unit);

        // Log level.
        const std::string log_level() const;
        void set_log_level(const std::string &log_level);

        // String representation of the context.
        const std::string repr() const;

        // Function to log a message at the specified log level. Needed for code that doesnt link to spdlog. (Python)
        void log(const std::string &level, const std::string &message);

        /*
            Pseudo-Private Methods.
            Not intended to be called,
            but I haven't set friend classes yet.
        */

        // Register a model with the context. This will automatically add it to the appropriate lists (modules, signals, etc.) with a dynamic_cast.
        void _add_model(Model *model);

        /*
            Take shared ownership of a model. The model will stay alive as long as the context does.
            Useful in python if a design is constructed in a function and the context is returned.
        */
        void _own_model(ModelPtr model);
        void _own_module(ModulePtr module);

        // Register a process with the context. This will create a Process object and set it as the active process.
        Process *register_process_func(const std::function<void()> &eval, Model *source, const std::string &name = "");

        // Thread-safe: schedule a signal for update. Called by Signal::write(), possibly from a
        // process running concurrently on the TMC thread pool. Defined inline so this hot path
        // (invoked on every signal write) can be inlined into Signal<T>::write() across TUs.
        void _schedule_signal_update(SignalBase *signal)
        {
            if (!_process_eval_parallel.load(std::memory_order_acquire))
            {
                _signal_update_stack.push_back(signal);
                return;
            }
            std::lock_guard lock(_signal_update_mutex);
            _signal_update_stack.push_back(signal);
        }
        // Thread-safe: cancel a previously scheduled signal update.
        void _unschedule_signal_update(SignalBase *signal)
        {
            if (!_process_eval_parallel.load(std::memory_order_acquire))
            {
                _signal_update_stack.erase(signal);
                return;
            }
            std::lock_guard lock(_signal_update_mutex);
            _signal_update_stack.erase(signal);
        }

        template <typename MemberFunc, typename ClassType>
        Process *register_method(MemberFunc mem_ptr, ClassType *instance, const std::string &name = "")
        {
            return register_process_func(method_to_function(mem_ptr, instance), static_cast<Model *>(instance), name);
        }

        /*
            The current hierarchal module being constructed.
        */
        Module *_active_module() const;

        /*
            The current, expanded hierarchy name.
        */
        const std::string _current_hierarchy() const;

    public:
        /*
            Static Methods
        */
        // Obtain the global context.
        static ContextPtr obtain();
        // Set the global context to nullptr. New designs will create a new context.
        static void reset();
        // Reset the global context, then obtain a new one.
        static ContextPtr create(const std::string &name = "");
    };

    class ContextFactory
    {
    private:
        int _next_context_id;
        ContextPtr _active_context;

    public:
        ContextFactory();

        // Obtain the current active context
        ContextPtr obtain();
        // Reset the active context.
        void reset();
        // Reset the global context, then obtain a new one.
        ContextPtr create(const std::string &name = "");
    };

    using ContextFactoryPtr = std::shared_ptr<ContextFactory>;
    ContextFactoryPtr get_global_context_factory();
    void set_global_context_factory(ContextFactoryPtr factory);
    void reset_global_context_factory();
}
