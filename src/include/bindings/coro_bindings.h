#pragma once
#include <dspsim/coro.h>
#include <dspsim/context.h>
#include "nb_include.h"

#include <stdexcept>
#include <string>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    /*
        Makes a C++ Task<T> awaitable from Python (`await axis_rx.receive(4, 100)`).

        The awaiting Python coroutine is driven by a PyTask process. Starting the C++ task runs it until its
        first leaf wait, which records the C++ coroutine as that process's resume point. Each time the
        scheduler resumes the Python coroutine, __await__ steps the C++ task from that resume point, until it
        is done and its result becomes the value of the await.

        Bindings return one of these for every coroutine method they expose: see bind_task() and py_task().
    */
    template <typename T>
    class PyTaskAwaitable
    {
    public:
        PyTaskAwaitable(Context *context, Task<T> task) : context_(context), task_(std::move(task)) {}

        void start()
        {
            process_ = context_->_current_process;
            if (process_ == nullptr)
            {
                throw std::runtime_error(
                    "A dspsim Task can only be awaited from a task running in the simulation "
                    "(Module.add_task() or Context.run_until()).");
            }
            task_.handle.resume();
        }
        bool done() const { return task_.done(); }
        void step() { process_->resume_point().resume(); }
        T result() { return task_.result(); }

    private:
        Context *context_;
        Task<T> task_;
        ProcessBase *process_ = nullptr;
    };

    // Wrap a task for Python. `context` is the context the task runs in (usually the module's).
    template <typename T>
    static inline PyTaskAwaitable<T> py_task(Context *context, Task<T> task)
    {
        return PyTaskAwaitable<T>(context, std::move(task));
    }

    /*
        Compiles a real Python generator function (it contains `yield`) so that __await__ can delegate to it:
        only genuine generator/coroutine objects get CPython's fast, (mostly) exception-free internal step path
        for `await`/`yield from`, unlike a custom C-extension iterator object.
    */
    static inline nb::object compile_generator(const char *source, const char *name)
    {
        nb::dict globals;
        nb::exec(nb::str(source), globals);
        return globals[name];
    }

    // Bind PyTaskAwaitable<T>. `result_type` is the Python type of the awaited value, for the stubs.
    template <typename T>
    static inline auto bind_task(nb::module_ &m, const char *name, const char *result_type = "None")
    {
        nb::object task_await = compile_generator(
            "def _dspsim_task_await(self):\n"
            "    self._py_start()\n"
            "    while not self._py_done():\n"
            "        yield\n"
            "        self._py_step()\n"
            "    return self._py_result()\n",
            "_dspsim_task_await");
        std::string sig = std::string("def __await__(self) -> typing.Generator[typing.Any, typing.Any, ") + result_type + "]";

        return nb::class_<PyTaskAwaitable<T>>(m, name)
            .def("_py_start", &PyTaskAwaitable<T>::start)
            .def("_py_done", &PyTaskAwaitable<T>::done)
            .def("_py_step", &PyTaskAwaitable<T>::step)
            .def("_py_result", &PyTaskAwaitable<T>::result)
            .def("__await__", [task_await](nb::object self)
                 { return task_await(self); }, nb::sig(sig.c_str()));
    }

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
            "    while not self._py_step_done():\n"
            "        yield\n"
            "    return self._py_result()\n",
            "_dspsim_wait_await");

        return nb::class_<WaitBase>(m, name)
            .def("_py_step_done", &WaitBase::py_step_done)
            .def("_py_result", [](WaitBase &)
                 { return nb::none(); })
            .def("__await__", [wait_await](nb::object self)
                 { return wait_await(self); }, nb::sig("def __await__(self) -> typing.Generator[typing.Any, typing.Any, typing.Any]"));
    }
    static inline auto bind_wait(nb::module_ &m, const char *name)
    {
        return nb::class_<Wait, WaitBase>(m, name)
            .def(nb::init<ProcessBase *>(), nb::arg("process"));
    }
    static inline auto bind_wait_time_event(nb::module_ &m, const char *name)
    {
        return nb::class_<WaitTimeEvent, WaitBase>(m, name)
            .def(nb::init<uint64_t, ProcessBase *>(), nb::arg("time_delta"), nb::arg("process"));
    }
    static inline auto bind_wait_sensitivity_event(nb::module_ &m, const char *name)
    {
        return nb::class_<WaitSensitivityEvent, WaitBase>(m, name)
            .def(nb::init<SensitivityEvent &, ProcessBase *, uint64_t>(), nb::arg("event"), nb::arg("process"), nb::arg("timeout") = 0)
            .def(nb::init<const std::vector<SensitivityEvent *> &, ProcessBase *, uint64_t>(), nb::arg("events"), nb::arg("process"), nb::arg("timeout") = 0)
            .def(nb::init<ProcessBase *>(), nb::arg("process"))
            .def("_py_result", &WaitSensitivityEvent::await_resume);
    }

} // namespace dspsim::bindings
