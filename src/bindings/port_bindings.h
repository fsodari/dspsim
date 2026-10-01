#pragma once
#include <dspsim/port.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;
    static inline auto bind_port_base(nb::module_ &m, const char *name)
    {
        return nb::class_<PortBase, Model>(m, name)
            .def_prop_ro("width", &PortBase::width)
            .def("change", &PortBase::change, nb::rv_policy::reference_internal)
            .def("pos", &PortBase::pos, nb::rv_policy::reference_internal)
            .def("neg", &PortBase::neg, nb::rv_policy::reference_internal);
    }

    template <typename T>
    static inline auto bind_input(nb::module_ &m, const char *name)
    {
        return nb::class_<Input<T>, PortBase>(m, name)
            // Don't need to "create". Ports will always exist inside a module.
            .def(nb::init<const std::string &>(), nb::arg("name"))
            // Methods
            .def("bind", &Input<T>::bind_signal, nb::arg("signal"))
            .def("bind", &Input<T>::bind_port, nb::arg("input"))
            .def("__call__", &Input<T>::bind_signal, nb::arg("signal"))
            .def("__call__", &Input<T>::bind_port, nb::arg("input"))
            .def("read", &Input<T>::read)
            .def_prop_ro("value", &Input<T>::read)
            .def_prop_ro("q", &Input<T>::read);
    }

    template <typename T>
    static inline auto bind_output(nb::module_ &m, const char *name)
    {
        return nb::class_<Output<T>, PortBase>(m, name)
            // Don't need to "create". Ports will always exist inside a module.
            .def(nb::init<const std::string &>(), nb::arg("name"))
            // Methods
            .def("bind", &Output<T>::bind_signal, nb::arg("signal"))
            .def("bind", &Output<T>::bind_port, nb::arg("output"))
            .def("__call__", &Output<T>::bind_signal, nb::arg("signal"))
            .def("__call__", &Output<T>::bind_port, nb::arg("output"))
            .def("write", &Output<T>::write, nb::arg("value"))
            .def_prop_rw("value", &Output<T>::read, &Output<T>::write, nb::arg("value"))
            .def_prop_rw("d", &Output<T>::read_d_, &Output<T>::write, nb::arg("value"))
            .def_prop_ro("q", &Output<T>::read);
    }

} // namespace dspsim::bindings
