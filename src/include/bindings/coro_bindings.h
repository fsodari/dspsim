#pragma once
#include <dspsim/coro.h>
// PyTaskAwaitable, py_task(), bind_task() and compile_generator(), which model extensions use as well.
#include <dspsim/bindings/task.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    static inline auto bind_wait_result(nb::module_ &m, const char *name)
    {
        return nb::enum_<WaitResult>(m, name)
            .value("Triggered", WaitResult::Triggered)
            .value("Timeout", WaitResult::Timeout);
    }

    static inline auto bind_wait_base(nb::module_ &m, const char *name)
    {
        // The leaf awaitables have a single suspend point: yield once, then resume with the wait's result.
        nb::object wait_await = compile_generator(
            "def _dspsim_wait_await(self):\n"
            "    yield\n"
            "    return self._py_result()\n",
            "_dspsim_wait_await");

        return nb::class_<WaitBase>(m, name)
            // Typed as object so that subclasses can override it with their own result type.
            .def("_py_result", [](WaitBase &)
                 { return nb::none(); }, nb::sig("def _py_result(self) -> object"))
            .def("__await__", [wait_await](nb::object self)
                 { return wait_await(self); }, nb::sig("def __await__(self) -> typing.Generator[typing.Any, typing.Any, typing.Any]"));
    }
    static inline auto bind_wait(nb::module_ &m, const char *name)
    {
        return nb::class_<Wait, WaitBase>(m, name);
    }
    static inline auto bind_wait_time_event(nb::module_ &m, const char *name)
    {
        return nb::class_<WaitTimeEvent, WaitBase>(m, name);
    }
    static inline auto bind_wait_sensitivity_event(nb::module_ &m, const char *name)
    {
        return nb::class_<WaitSensitivityEvent, WaitBase>(m, name)
            .def("_py_result", &WaitSensitivityEvent::await_resume);
    }

} // namespace dspsim::bindings
