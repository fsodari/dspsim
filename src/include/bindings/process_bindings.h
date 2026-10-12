#pragma once
#include <dspsim/event.h>
#include <dspsim/port.h>
#include <dspsim/process.h>
#include <dspsim/signal.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    static inline Process *process_always_func(Process *self, nb::args args)
    {
        for (auto arg : args)
        {
            if (nb::isinstance<SensitivityEvent &>(arg))
            {
                self->always(nb::cast<SensitivityEvent &>(arg));
            }
            else if (nb::isinstance<PortBase>(arg))
            {
                self->always(nb::cast<PortBase &>(arg).change());
            }
            else if (nb::isinstance<SignalBase>(arg))
            {
                self->always(nb::cast<SignalBase &>(arg).change());
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
            .def("initialize", [](ProcessBase *self, bool init)
                 { return self->initialize(init); }, nb::arg("init"), nb::rv_policy::reference_internal)
            .def("always", [](ProcessBase *self, const std::string &event_name)
                 { return self->always(event_name); }, nb::arg("event_name"), nb::rv_policy::reference_internal)
            .def("always", &process_always_func, nb::rv_policy::reference_internal, nb::sig("def always(self, *args: SensitivityEvent | PortBase | SignalBase) -> ProcessBase"))
            .def("disable", [](ProcessBase *self)
                 { return self->disable(); }, nb::rv_policy::reference_internal)
            .def("enable", [](ProcessBase *self)
                 { return self->enable(); }, nb::rv_policy::reference_internal)
            .def_prop_ro("done", &ProcessBase::done);
    }

    static inline auto bind_process(nb::module_ &m, const char *name)
    {
        return nb::class_<Process, ProcessBase>(m, name);
    }

} // namespace dspsim::bindings
