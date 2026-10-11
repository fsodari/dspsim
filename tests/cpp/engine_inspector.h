/*
    Read-only view of engine internals for the tests.

    Tests assert on observable behavior through the public API. Use the inspector only for invariants that
    have no observable effect: cleanup (no process or subscription left behind) and "this process did not run".
    The core classes befriend this struct, and it is defined here, in the test tree, so nothing is shipped.
    Every query is const: a test can look at the engine, but never put it in a state the public API can't reach.
*/
#pragma once
#include <dspsim/dspsim.h>

#include <cstddef>
#include <cstdint>

namespace dspsim::testing
{
    struct EngineInspector
    {
        /// Number of processes registered with the context.
        static std::size_t process_count(const Context &context) { return context.processes_.size(); }
        /// Number of scheduled time events, including stale ones that will be dropped when due.
        static std::size_t pending_time_events(const Context &context) { return context.time_event_stack_.size(); }
        /// The module under construction, or nullptr.
        static Module *active_module(const Context &context) { return context.active_module(); }

        /// Number of times the scheduler has resumed the process.
        static uint64_t wake_count(const ProcessBase &process) { return process.wake_count(); }

        static bool has_static_subscribers(const SensitivityEvent &event) { return event.has_static_subscribers(); }
        static bool has_dynamic_subscribers(const SensitivityEvent &event) { return event.has_dynamic_subscribers(); }
    };
} // namespace dspsim::testing
