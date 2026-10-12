#pragma once
#include <dspsim/event.h>
#include <dspsim/utils/flagged_stack.h>
#include <dspsim/utils/priority_queue.h>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dspsim
{
    class Model;
    class Module;
    class ModuleName;
    class SignalBase;
    class Channel;
    class PortBase;
    class ProcessBase;
    // Coroutine types, defined in <dspsim/coro.h>. Include it to call wait() or run_until().
    template <typename T>
    struct Task;
    class Wait;
    class WaitTimeEvent;
    class WaitSensitivityEvent;

    namespace detail
    {
        struct EngineAccess;
        // Called at the end of the update phase: derived signals and channels. Defined in <dspsim/channel.h>.
        class Updatable;
        // The engine's logger. Defined in src/log.h, which is internal to the core library.
        class Logger;
    }
    namespace testing
    {
        // Read-only view of engine internals for the tests. Defined in tests/cpp/engine_inspector.h.
        struct EngineInspector;
    }

    /// Thrown when a new context is created while the active context has not been elaborated.
    class ContextConstructionError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    /// Thrown by Context::run_until() when the task does not complete within its timeout.
    class TimeoutError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    /*
        Context contains a vector of all the models.
        Responsible for elaboration, simulation, and management of models.

        Context states:
        - Design setup. Models can be added to the context. Models self-register with the global active context, so
          only one context can be under construction at a time. Context::create() takes a construction lock: another
          thread calling create() blocks until the context is elaborated or released, and the same thread calling
          create() again throws ContextConstructionError, since waiting would deadlock.
        - Released. Once elaboration is complete, the context is released from the global factory.
          The context can be used for simulation, but no new models can be added to it.
          New contexts can be created with new parameters so that they can run in parallel.
    */
    class Context
    {
        // The engine classes schedule themselves on the context and register with it on construction.
        friend class ContextFactory;
        friend class Model;
        friend class Module;
        friend class ModuleName;
        friend class PortBase;
        friend class SignalBase;
        template <typename T>
        friend class Signal;
        friend class ProcessBase;
        friend class SensitivityEvent;
        // A channel asks for its update in the update phase.
        friend class Channel;
        // Engine access for code outside the core library (the Python bindings) and for the tests.
        friend struct detail::EngineAccess;
        friend struct testing::EngineInspector;

    public:
        // Explicitly delete copy constructor and assignment
        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;

        // Move operations can be kept if desired
        Context(Context &&) noexcept;
        ~Context();

        // Obtain the active global context. Throws ContextConstructionError if there is none.
        static std::shared_ptr<Context> obtain();
        // Set the global context to nullptr. New designs must call create().
        static void reset();
        // Create a new global context. Throws ContextConstructionError if the active context is not elaborated.
        static std::shared_ptr<Context> create(const std::string &name = "");

        /*
            Clear all models from the context.
        */
        void clear();

        /*
            Call elaborate after construction is complete.
            This will finalize all port bindings,
            and TODO: check for any netlist violations.
            The design is then locked and the context is released from the global context factory,
            so a new, independent context can be created. The context is released even if elaboration fails.
        */
        void elaborate();

        // True once elaborate() has been called.
        bool elaborated() const;

        /*
            Stop being the global active context and release the construction lock, if this context is the active one.
            Does nothing if another context is active.
        */
        void release();
        // True while this context is the active context under construction.
        bool constructing() const;

        /*
            Run the simulation for the given time increment.
            If time_inc is 0, it runs delta cycles until the design settles without advancing time.
        */
        void run(uint64_t time_inc = 0);

        /*
            Run the simulation until the task completes and return its value. The task is registered as a
            one-shot process, scheduled immediately, and removed once done. Time advances event-to-event, like run().
            Throws TimeoutError if the task is not done after `timeout` time units (0 = no timeout), with the
            simulation left at the deadline, and std::runtime_error if the simulation stalls (no pending time
            events) before the task completes. An exception thrown by the task propagates.

            This is the entry point for asynchronous test code from ordinary (non-coroutine) code:

                auto rx = ctx->run_until(axis_rx.receive(16, 1000));
        */
        template <typename T>
        T run_until(Task<T> task, uint64_t timeout = 0);

        // Run the simulation until the event triggers. Returns false if the timeout (0 = none) elapsed first.
        bool run_until(SensitivityEvent &event, uint64_t timeout = 0);

        /*
            Coroutine awaitables. The process defaults to the one currently being evaluated, so inside a
            coroutine process these can be awaited directly: `co_await ctx->wait(10);`.
        */
        // wait on all events in the static sensitivity list.
        Wait wait();
        // Wait on a time event.
        WaitTimeEvent wait(uint64_t time_delta);

        // wait on a dynamic event.
        WaitSensitivityEvent wait(SensitivityEvent &event);
        // wait on multiple dynamic events.
        WaitSensitivityEvent wait(const std::vector<std::reference_wrapper<SensitivityEvent>> &events);
        // wait on multiple dynamic events, given by pointer.
        WaitSensitivityEvent wait(const std::vector<SensitivityEvent *> &events);

        /*
            Wait on dynamic events with a timeout (> 0). The result says whether an event triggered or the
            timeout elapsed. A literal 0 timeout is ambiguous with the process argument: use the overloads above.
        */
        WaitSensitivityEvent wait(SensitivityEvent &event, uint64_t timeout);
        WaitSensitivityEvent wait(const std::vector<std::reference_wrapper<SensitivityEvent>> &events, uint64_t timeout);
        WaitSensitivityEvent wait(const std::vector<SensitivityEvent *> &events, uint64_t timeout);

        // Log the model hierarchy, starting from the given parent (nullptr = roots).
        void print_hierarchy(Model *parent = nullptr, int depth = 0) const;

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
            Take shared ownership of an object: it stays alive until the context is cleared or destroyed.
            For anything the context or its processes refer to that nothing else keeps alive: a model that
            is created in a function which only returns the context (Model::create() does this), an event
            or a channel that is created from Python.
        */
        void own_object(std::shared_ptr<void> object);

        // Register a function as a process.
        ProcessBase *register_process_func(const std::function<void()> &eval, const std::string &name = "");

        /*
            Register a member function bound at compile time: register_method<&MyModule::eval>(this).
            This is what DSPSIM_METHOD uses. The call is a plain function pointer call with no allocation.
        */
        template <auto Method, typename ClassType>
        ProcessBase *register_method(ClassType *instance, const std::string &name = "")
        {
            return register_trampoline(
                instance, [](void *p)
                { (static_cast<ClassType *>(p)->*Method)(); },
                name);
        }

        // Register a coroutine task as a process. The process owns the coroutine frame.
        // The process lives as long as the context, so the returned pointer stays valid after the task has
        // completed (done()). For a task that is created while the simulation runs, see spawn().
        template <typename T>
        ProcessBase *register_coro_task(Task<T> task, const std::string &name = "")
        {
            return register_coro(task.release(), name);
        }

        /*
            Start a coroutine task as a process of its own and let it go: the context destroys the process
            when the task has completed, so there is no handle to it, and its result is discarded.
            This is for work that is created while the simulation runs, a task per transaction for example,
            which would otherwise accumulate. (SystemC: sc_spawn.)

            Like any process, the task is evaluated in the delta cycle in which it is spawned when a process
            spawns it, and at the start of the next run otherwise.
        */
        template <typename T>
        void spawn(Task<T> task, const std::string &name = "")
        {
            spawn_coro(task.release(), name);
        }

    private:
        // Can't create context directly. Use create() to make a new global context, and obtain() to get the active one.
        Context(const std::string &name, int id);

        // Register a model with the context. This will automatically add it to the appropriate lists (modules, signals, etc.) with a dynamic_cast.
        void add_model(Model *model);
        void add_module(Module *module);
        void add_signal(SignalBase *signal);

        // Request dump_trace() calls on the model at the end of each delta cycle. See Model::request_trace().
        void trace_model(Model *model);

        // Each model is assigned a unique id from its context on construction.
        uint32_t next_model_id();
        uint32_t next_process_id();

        /*
            Take ownership of a process and schedule it for evaluation in the next delta cycle
            (see ProcessBase::initialize()). Processes can be registered at any time: during construction,
            between run() calls, or from a running process.
        */
        ProcessBase *add_process(std::unique_ptr<ProcessBase> process);
        ProcessBase *register_trampoline(void *instance, void (*call)(void *), const std::string &name);
        ProcessBase *register_coro(std::coroutine_handle<> root, const std::string &name);
        void spawn_coro(std::coroutine_handle<> root, const std::string &name);
        // add_process() for a process that the context destroys when it has completed. See spawn().
        void add_detached_process(std::unique_ptr<ProcessBase> process);
        // Destroy the spawned processes that have completed.
        void release_completed_processes();
        // Evaluate the process again in the next delta cycle: a wait or a next_trigger() of no time.
        void wait_for_next_delta(ProcessBase *process);

        /*
            Run the simulation until the (already registered) process is done. See run_until().
            Registering the process scheduled it; the caller removes it afterwards with remove_process().
        */
        void run_until_done(ProcessBase *process, uint64_t timeout = 0);

        // Unregister a process: unsubscribe it from its dynamic events, drop its pending time events, destroy it.
        void remove_process(ProcessBase *process);

        // The process being evaluated, or nullptr outside the eval phase.
        // Inline, like update_count(): read by the scheduler and the awaitables for every evaluation.
        ProcessBase *current_process() const { return current_process_; }

        /*
            The current hierarchal module being constructed.
        */
        Module *active_module() const;

        /*
            The current, expanded hierarchy name.
        */
        const std::string current_hierarchy() const;

        // Number of delta cycles so far. Used to handle event flags.
        int64_t update_count() const { return update_count_; }

        // Run delta cycles until no process or signal is scheduled. Returns the number of delta cycles.
        int eval();
        // Commit the initial signal values before the first evaluation.
        void do_initialize();
        // Commit all scheduled signal updates, then recompute the derived signals of the signals that changed
        // and update the channels that asked for it.
        void update_signals();
        // Queue the process of a due time event, unless the event is a stale coroutine time wait.
        void schedule_time_event(const TimeEvent &event);
        // Module::next_trigger(time_delta): evaluate the running process again after the given time delta.
        // Throws std::logic_error in a coroutine process: use wait(time_delta) instead.
        void schedule_time_delta_event(uint64_t time_delta);

    private:
        // Context info
        std::string name_;
        int id_;
        // Each model is assigned a unique ID, starting from 0.
        uint32_t next_model_id_;
        // List of all registered models, modules, and signals.
        std::vector<Model *> registered_models_;
        std::vector<Module *> modules_;
        std::vector<SignalBase *> signals_;

        // Each process is assigned a unique ID, starting from 0.
        uint32_t next_process_id_;

        // Processes are allocated from functions, so they need to live somewhere.
        std::vector<std::unique_ptr<ProcessBase>> processes_;

        // Objects that stay alive with the context: see own_object().
        std::vector<std::shared_ptr<void>> owned_objects_;

        // Design hierarchy: maps a model to its direct children (root models are keyed by nullptr).
        std::unordered_map<Model *, std::vector<Model *>> children_;

        // Current simulation time.
        uint64_t time_;
        // Time unit used for tracing.
        std::string time_unit_;
        bool initialized_ = false;
        // Set by elaborate(). The design is locked once elaborated.
        bool elaborated_ = false;
        // Incremented every time a delta cycle occurs. Used to handle event flags.
        int64_t update_count_;

        // All processes that need to run in the current delta cycle.
        FlaggedStack<ProcessBase *> process_eval_stack_;
        ProcessBase *current_process_;
        // Spawned processes that have completed. Destroyed when the delta cycles of the time step are over.
        // Here, with the scheduling stacks: eval() looks at it every time, and the members above are
        // only used at elaboration, so their cache line is not one the run cycle keeps loaded.
        std::vector<ProcessBase *> completed_processes_;

        // All signals that need to be updated in the current delta cycle.
        FlaggedStack<SignalBase *> signal_update_stack_;
        // What to call after the signal updates in the current delta cycle: derived signals to recompute
        // and channels to update.
        FlaggedStack<detail::Updatable *> post_update_stack_;
        // Notified to end a wait of no time: the processes waiting on it are evaluated in the next delta cycle.
        std::unique_ptr<SensitivityEvent> next_delta_event_;
        // Scheduled sensitivity events.
        PlainStack<SensitivityEvent *> sensitivity_event_stack_;
        // Scheduled time events.
        PriorityQueue<TimeEvent> time_event_stack_;
        // Stack of models that have requested tracing. Evaluated at end of a delta cycle.
        std::vector<Model *> trace_stack_;

        // The logging backend (spdlog) is an implementation detail: use log() and set_log_level().
        std::unique_ptr<detail::Logger> logger_;

        // Keep track of the currently active module to build a hierarchy.
        std::vector<Module *> active_module_stack_;
        // Names of the ModuleName scopes being constructed. Module() takes its name from the top.
        std::vector<std::string> active_module_name_stack_;
    };

    /*
        Tracks the active context, the one under construction that new models register with.
        Only one context can be under construction at a time, and only by the thread that created it.
    */
    class ContextFactory
    {
    public:
        ContextFactory();
        ~ContextFactory();

        /*
            Obtain the active context. Throws ContextConstructionError if this thread has no context under construction:
            create() must be called first, and the context is no longer active once it has been elaborated or released.
        */
        std::shared_ptr<Context> obtain();
        // Discard the active context and release the construction lock, from any thread.
        void reset();
        // Release the construction lock, if the given context is the active one.
        void release(const Context *context);
        // True if the given context is the active one.
        bool is_active(const Context *context);
        /*
            Create a new context and make it the active one. Blocks while another thread has a context under construction.
            Throws ContextConstructionError if this thread already has one: elaborate or release it first,
            or call reset() to discard it.
        */
        std::shared_ptr<Context> create(const std::string &name = "");

    private:
        // Release the active context (and its construction lock) if the predicate accepts it.
        void release_active(const std::function<bool(const Context *)> &if_active);

    private:
        // The active context, the thread constructing it and the lock that guards them. Defined in context.cpp,
        // which keeps <mutex>, <thread> and <condition_variable> out of every file that includes this header.
        struct State;
        std::unique_ptr<State> state_;
    };

    using ContextFactoryPtr = std::shared_ptr<ContextFactory>;
    std::shared_ptr<ContextFactory> get_global_context_factory();
    void set_global_context_factory(std::shared_ptr<ContextFactory> factory);
    void reset_global_context_factory();

    template <typename T>
    T Context::run_until(Task<T> task, uint64_t timeout)
    {
        // The process takes ownership of the frame. Keep the typed handle to read the result once done.
        auto handle = task.handle;
        ProcessBase *process = register_coro_task(std::move(task), "run_until");
        // Remove the process however run_until_done() exits. The frame is destroyed with the process.
        struct Cleanup
        {
            Context *context;
            ProcessBase *process;
            ~Cleanup() { context->remove_process(process); }
        } cleanup{this, process};

        run_until_done(process, timeout);
        return handle.promise().result();
    }
}
