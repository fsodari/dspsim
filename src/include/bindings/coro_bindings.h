#pragma once
#include <dspsim/coro.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    static inline auto bind_wait_base(nb::module_ &m, const char *name)
    {
        /*
            Compiles a real Python generator function (it contains `yield`) so that
            __await__ can delegate to it below: only genuine generator/coroutine
            objects get CPython's fast, (mostly) exception-free internal step path for
            `await`/`yield from`, unlike a custom C-extension iterator object.

            Hacking.
         */
        nb::dict globals;
        nb::exec(
            "def _dspsim_wait_await(self):\n"
            "    while not self._py_step_done():\n"
            "        yield\n",
            globals);
        nb::object dspsim_wait_await = globals["_dspsim_wait_await"];

        return nb::class_<WaitBase>(m, name)
            .def("_py_step_done", &WaitBase::py_step_done)
            .def("__await__", [dspsim_wait_await](nb::object self)
                 { return dspsim_wait_await(self); }, nb::sig("def __await__(self) -> typing.Generator[typing.Any, typing.Any, None]"));
    }
    static inline auto bind_wait(nb::module_ &m, const char *name)
    {
        return nb::class_<Wait, WaitBase>(m, name)
            .def(nb::init<>());
    }
    static inline auto bind_wait_time_event(nb::module_ &m, const char *name)
    {
        return nb::class_<WaitTimeEvent, WaitBase>(m, name)
            .def(nb::init<uint64_t, ProcessBase *>(), nb::arg("time_delta"), nb::arg("process"));
    }
    static inline auto bind_wait_sensitivity_event(nb::module_ &m, const char *name)
    {
        return nb::class_<WaitSensitivityEvent, WaitBase>(m, name)
            .def(nb::init<std::reference_wrapper<SensitivityEvent>, ProcessBase *>(), nb::arg("event"), nb::arg("process"))
            .def(nb::init<std::vector<std::reference_wrapper<SensitivityEvent>>, ProcessBase *>(), nb::arg("events"), nb::arg("process"))
            .def(nb::init<ProcessBase *>(), nb::arg("process"));
    }

} // namespace dspsim::bindings
