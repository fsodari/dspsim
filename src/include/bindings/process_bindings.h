#pragma once
#include <dspsim/process.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    static inline Process *_process_always_func(Process *self, nb::args args)
    {
        for (auto arg : args)
        {
            if (nb::isinstance<SensitivityEvent &>(arg))
            {
                self->schedule_static_event(nb::cast<SensitivityEvent &>(arg));
            }
            else if (nb::isinstance<PortBase>(arg))
            {
                self->schedule_static_event(nb::cast<PortBase &>(arg).change());
            }
            else if (nb::isinstance<SignalBase>(arg))
            {
                self->schedule_static_event(nb::cast<SignalBase &>(arg).change());
            }
            else
            {
                throw std::runtime_error("Unsupported argument type for SensitivityList");
            }
        }
        return self;
    }

    static inline auto bind_process_base(nb::module_ &m, const char *name)
    {
        return nb::class_<ProcessBase>(m, name)
            .def_prop_ro("context", &ProcessBase::context, nb::rv_policy::reference_internal)
            .def_prop_ro("id", &ProcessBase::id)
            .def_prop_ro("name", &ProcessBase::name)
            .def("initialize", &ProcessBase::_set_initialize, nb::arg("init"), nb::rv_policy::reference_internal)
            .def("always", &ProcessBase::_always_str, nb::arg("event_name"), nb::rv_policy::reference_internal)
            .def("always", &_process_always_func, nb::rv_policy::reference_internal, nb::sig("def always(self, *args: SensitivityEvent | InputBase | SignalBase) -> ProcessBase"))
            .def_prop_ro("done", &ProcessBase::done);
    }

    static inline auto bind_process(nb::module_ &m, const char *name)
    {
        return nb::class_<Process, ProcessBase>(m, name);
    }

} // namespace dspsim::bindings
