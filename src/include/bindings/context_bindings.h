#pragma once
#include <dspsim/context.h>
#include "coro_bindings.h"
#include "py_task.h"
#include "nb_include.h"

#include <memory>

namespace dspsim::bindings
{
    namespace nb = nanobind;
    // Run a Python awaitable (a coroutine object, or anything with __await__) as a one-shot task to completion.
    static inline nb::object _context_run_until_awaitable(nb::object run_awaitable, Context &ctx, nb::object awaitable, uint64_t timeout)
    {
        // Wrapping in a coroutine accepts any awaitable, e.g. a C++ task such as axis_rx.receive(4, 100).
        nb::object coro = run_awaitable(awaitable);
        auto task = std::make_unique<PyTask>(&ctx, std::move(coro), "run_until");
        PyTask *process = task.get();
        ctx._processes.push_back(std::move(task));
        // Remove the process however _run_until_done() exits.
        struct Cleanup
        {
            Context *context;
            ProcessBase *process;
            ~Cleanup() { context->_remove_process(process); }
        } cleanup{&ctx, process};

        ctx._run_until_done(process, timeout);
        return process->result();
    }

    static inline auto bind_context(nb::module_ &m, const char *name)
    {
        nb::exception<TimeoutError>(m, "TimeoutError", PyExc_TimeoutError);

        nb::dict globals;
        nb::exec(
            "async def _dspsim_run_until(awaitable):\n"
            "    return await awaitable\n",
            globals);
        nb::object run_awaitable = globals["_dspsim_run_until"];

        // Bind the Context class
        return nb::class_<Context>(m, name)
            // create() blocks while another thread is constructing a context, so it must not hold the GIL.
            .def(nb::new_(&Context::create), nb::arg("name") = "", nb::call_guard<nb::gil_scoped_release>())
            // Methods
            .def("clear", &Context::clear)
            .def("elaborate", &Context::elaborate)
            .def("release", &Context::release)
            .def("eval", &Context::eval)
            .def("run", &Context::run, nb::arg("time_inc") = 0)
            // Run until the event triggers. False if the timeout (> 0) elapsed first.
            .def("run_until", [](Context &ctx, SensitivityEvent &event, uint64_t timeout)
                 { return ctx.run_until(event, timeout); }, nb::arg("event"), nb::arg("timeout") = 0)
            // Run until the awaitable completes and return its value. Raises TimeoutError if the timeout (> 0) elapses first.
            .def("run_until", [run_awaitable](Context &ctx, nb::object awaitable, uint64_t timeout)
                 { return _context_run_until_awaitable(run_awaitable, ctx, std::move(awaitable), timeout); }, nb::arg("task"), nb::arg("timeout") = 0, nb::sig("def run_until(self, task: typing.Awaitable[typing.Any], timeout: int = 0) -> typing.Any"))
            .def("schedule_time_delta_event", &Context::schedule_time_delta_event, nb::arg("time_delta"), nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx, ProcessBase *process)
                 { return ctx.wait(process); }, nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx, uint64_t time_delta, ProcessBase *process)
                 { return ctx.wait(time_delta, process); }, nb::arg("time_delta"), nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx, SensitivityEvent &event, ProcessBase *process)
                 { return ctx.wait(event, process); }, nb::arg("event"), nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx, const std::vector<SensitivityEvent *> &events, ProcessBase *process)
                 { return ctx.wait(events, process); }, nb::arg("events"), nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx, SensitivityEvent &event, uint64_t timeout, ProcessBase *process)
                 { return ctx.wait(event, timeout, process); }, nb::arg("event"), nb::arg("timeout"), nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx, const std::vector<SensitivityEvent *> &events, uint64_t timeout, ProcessBase *process)
                 { return ctx.wait(events, timeout, process); }, nb::arg("events"), nb::arg("timeout"), nb::arg("process") = nullptr)
            .def("print_hierarchy", &Context::print_hierarchy, nb::arg("parent").none() = nullptr, nb::arg("depth") = 0)
            .def("children", &Context::children, nb::arg("parent").none())

            // Properties
            .def_prop_ro("name", &Context::name)
            .def_prop_ro("id", &Context::id)
            .def_prop_ro("elaborated", &Context::elaborated)
            .def_prop_ro("constructing", &Context::constructing)
            .def_prop_ro("models", &Context::models)
            .def_prop_ro("owned_models", &Context::owned_models)
            .def_prop_ro("modules", &Context::modules)
            .def_prop_ro("signals", &Context::signals)

            .def_prop_ro("time", &Context::time)
            .def_prop_rw("time_unit", &Context::time_unit, &Context::set_time_unit)
            .def_prop_rw("log_level", &Context::log_level, &Context::set_log_level)

            .def("__repr__", &Context::repr)
            .def("__str__", &Context::repr)

            // Python module base class will need to explicitly call this.
            .def("own_model", &Context::_own_model)
            // .def("own_module", &Context::_own_module, nb::arg("module"))

            // Register a process.
            .def("register_process", &Context::register_process_func, nb::arg("func"), nb::arg("name") = "", nb::rv_policy::reference)

            // Static Methods
            .def_static("obtain", &Context::obtain)
            .def_static("reset", &Context::reset)
            .def_static("create", &Context::create, nb::arg("name") = "", nb::call_guard<nb::gil_scoped_release>());
    }

    static inline auto bind_context_factory(nb::module_ &m, const char *name)
    {
        nb::exception<ContextConstructionError>(m, "ContextConstructionError", PyExc_RuntimeError);
        m.def("set_global_context_factory", &set_global_context_factory, nb::arg("context_factory"));
        m.def("get_global_context_factory", &get_global_context_factory);
        m.def("reset_global_context_factory", &reset_global_context_factory);

        return nb::class_<ContextFactory>(m, name)
            .def("obtain", &ContextFactory::obtain)
            .def("reset", &ContextFactory::reset)
            .def("release", &ContextFactory::release, nb::arg("context"))
            .def("is_active", &ContextFactory::is_active, nb::arg("context"))
            .def("create", &ContextFactory::create, nb::arg("name") = "", nb::call_guard<nb::gil_scoped_release>());
    }
} // namespace dspsim