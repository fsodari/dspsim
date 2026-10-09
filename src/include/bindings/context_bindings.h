#pragma once
#include <dspsim/context.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;
    static inline auto bind_context(nb::module_ &m, const char *name)
    {
        // Bind the Context class
        return nb::class_<Context>(m, name)
            .def(nb::new_(&Context::create), nb::arg("name") = "")
            // Methods
            .def("clear", &Context::clear)
            .def("elaborate", &Context::elaborate)
            .def("eval", &Context::eval)
            .def("run", &Context::run, nb::arg("time_inc") = 0)
            .def("schedule_time_delta_event", &Context::schedule_time_delta_event, nb::arg("time_delta"), nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx)
                 { return ctx.wait(); })
            .def("wait", [](Context &ctx, uint64_t time_delta, ProcessBase *process)
                 { return ctx.wait(time_delta, process); }, nb::arg("time_delta"), nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx, SensitivityEvent &event, ProcessBase *process)
                 { return ctx.wait(event, process); }, nb::arg("event"), nb::arg("process") = nullptr)
            .def("wait", [](Context &ctx, std::vector<std::reference_wrapper<SensitivityEvent>> events, ProcessBase *process)
                 { return ctx.wait(events, process); }, nb::arg("events"), nb::arg("process") = nullptr)
            .def("print_hierarchy", &Context::print_hierarchy, nb::arg("parent").none() = nullptr, nb::arg("depth") = 0)
            .def("children", &Context::children, nb::arg("parent").none())

            // Properties
            .def_prop_ro("name", &Context::name)
            .def_prop_ro("id", &Context::id)
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
            .def_static("create", &Context::create, nb::arg("name") = "");
    }

    static inline auto bind_context_factory(nb::module_ &m, const char *name)
    {
        m.def("set_global_context_factory", &set_global_context_factory, nb::arg("context_factory"));
        m.def("get_global_context_factory", &get_global_context_factory);
        m.def("reset_global_context_factory", &reset_global_context_factory);

        return nb::class_<ContextFactory>(m, name)
            .def("obtain", &ContextFactory::obtain)
            .def("reset", &ContextFactory::reset)
            .def("create", &ContextFactory::create, nb::arg("name") = "");
    }
} // namespace dspsim