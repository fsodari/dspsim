#pragma once
#include <coroutine>
#include <cstdint>
#include <memory>

namespace dspsim
{
    class Context;
    class ProcessBase;

    namespace detail
    {
        /*
            Access to engine internals for code that extends the engine from outside the core library: the Python
            bindings register their own process type (PyTask) and drive C++ tasks from Python coroutines.
            Context and ProcessBase befriend this struct, so every use of their internals from outside the core
            goes through the functions below. Not part of the user API.
        */
        struct EngineAccess
        {
            /// Take ownership of a process and schedule it for evaluation in the next delta cycle.
            static ProcessBase *add_process(Context &context, std::unique_ptr<ProcessBase> process);
            /// Unregister and destroy a process added with add_process().
            static void remove_process(Context &context, ProcessBase *process);
            /// Run the simulation until the registered process is done. See Context::run_until().
            static void run_until_done(Context &context, ProcessBase *process, uint64_t timeout = 0);
            /// The process being evaluated, or nullptr outside the eval phase.
            static ProcessBase *current_process(const Context &context);
            /// The suspended coroutine the scheduler would resume next for this process.
            static std::coroutine_handle<> resume_point(const ProcessBase &process);
        };
    } // namespace detail
} // namespace dspsim
