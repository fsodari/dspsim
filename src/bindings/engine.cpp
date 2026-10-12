#include "bindings/framework.h"

#include "bindings/context_bindings.h"
#include "bindings/model_bindings.h"
#include "bindings/event_bindings.h"
#include "bindings/channel_bindings.h"
#include "bindings/process_bindings.h"
#include "bindings/py_task.h"
#include "bindings/coro_bindings.h"
#include "bindings/module_name_bindings.h"
#include "bindings/module_bindings.h"

namespace dspsim::bindings
{
    void register_engine(nb::module_ &m)
    {
        // Bind the Context
        bind_context(m, "Context");

        // Bind ContextFactory and global context factory functions
        bind_context_factory(m, "ContextFactory");

        // Bind Model base class
        bind_model(m, "Model");

        // Bind SensitivityEvent
        bind_sensitivity_event(m, "SensitivityEvent");

        // Bind Channel, the base class of the channels that are written in Python
        bind_channel(m, "Channel");

        // Bind Process
        bind_process_base(m, "ProcessBase");
        bind_process(m, "Process");

        // Python version of coroutines.
        bind_py_task(m, "PyTask");

        // Awaitable bindings
        bind_wait_result(m, "WaitResult");
        bind_wait_base(m, "WaitBase");
        bind_wait(m, "Wait");
        bind_wait_time_event(m, "WaitTimeEvent");
        bind_wait_sensitivity_event(m, "WaitSensitivityEvent");

        // C++ tasks awaitable from Python, one per result type.
        bind_task<void>(m, "Task", "None");
        bind_task<bool>(m, "TaskBool", "bool");
        bind_task<std::vector<uint8_t>>(m, "TaskListU8", "list[int]");
        bind_task<std::vector<uint16_t>>(m, "TaskListU16", "list[int]");
        bind_task<std::vector<uint32_t>>(m, "TaskListU32", "list[int]");
        bind_task<std::vector<uint64_t>>(m, "TaskListU64", "list[int]");
        bind_task<std::vector<int8_t>>(m, "TaskListS8", "list[int]");
        bind_task<std::vector<int16_t>>(m, "TaskListS16", "list[int]");
        bind_task<std::vector<int32_t>>(m, "TaskListS32", "list[int]");
        bind_task<std::vector<int64_t>>(m, "TaskListS64", "list[int]");

        // Module
        bind_module_name(m, "ModuleName");
        bind_module(m, "Module");
    }
} // namespace dspsim::bindings
