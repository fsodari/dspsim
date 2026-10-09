#pragma once
#include <dspsim/port.h>
#include "nb_include.h"
#include "ndarray_bindings.h"

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
            .def(nb::init<const std::string &, int>(),
                 nb::arg("name"),
                 nb::arg("width") = default_bitwidth<T>::value)
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
            .def(nb::init<const std::string &, int>(),
                 nb::arg("name"),
                 nb::arg("width") = default_bitwidth<T>::value)
            // Methods
            .def("bind", &Output<T>::bind_signal, nb::arg("signal"))
            .def("bind", &Output<T>::bind_port, nb::arg("output"))
            .def("__call__", &Output<T>::bind_signal, nb::arg("signal"))
            .def("__call__", &Output<T>::bind_port, nb::arg("output"))
            .def("write", &Output<T>::write, nb::arg("value"))
            .def("read", &Output<T>::read)
            .def_prop_rw("value", &Output<T>::read, &Output<T>::write, nb::arg("value"))
            .def_prop_rw("d", &Output<T>::read_d_, &Output<T>::write, nb::arg("value"))
            .def_prop_ro("q", &Output<T>::read);
    }

    template <typename T>
    static inline auto bind_input_array_view(nb::module_ &m, const char *name)
    {
        using View = InputArrayView<T>;
        auto cls = nb::class_<View>(m, name)
                       .def("bind", nb::overload_cast<const SignalArrayView<T> &>(&View::bind_signal), nb::arg("signals"))
                       .def("bind", nb::overload_cast<const SignalArray<T> &>(&View::bind_signal), nb::arg("signals"))
                       .def("bind", nb::overload_cast<const InputArrayView<T> &>(&View::bind_port), nb::arg("ports"))
                       .def("bind", nb::overload_cast<const InputArray<T> &>(&View::bind_port), nb::arg("ports"))
                       .def("__call__", nb::overload_cast<const SignalArrayView<T> &>(&View::bind_signal), nb::arg("signals"))
                       .def("__call__", nb::overload_cast<const SignalArray<T> &>(&View::bind_signal), nb::arg("signals"))
                       .def("__call__", nb::overload_cast<const InputArrayView<T> &>(&View::bind_port), nb::arg("ports"))
                       .def("__call__", nb::overload_cast<const InputArray<T> &>(&View::bind_port), nb::arg("ports"));
        bind_ndarray_common<View>(cls, name);
        return cls;
    }

    template <typename T>
    static inline auto bind_input_array(nb::module_ &m, const char *name)
    {
        using Array = InputArray<T>;
        auto cls = nb::class_<Array>(m, name)
                       .def(nb::init<const std::string &, Shape, int>(),
                            nb::arg("name"), nb::arg("shape"), nb::arg("width") = default_bitwidth<T>::value)
                       .def("bind", nb::overload_cast<SignalArray<T> &>(&Array::bind_signal), nb::arg("signals"))
                       .def("bind", nb::overload_cast<const SignalArrayView<T> &>(&Array::bind_signal), nb::arg("signals"))
                       .def("bind", nb::overload_cast<InputArray<T> &>(&Array::bind_port), nb::arg("inputs"))
                       .def("bind", nb::overload_cast<const InputArrayView<T> &>(&Array::bind_port), nb::arg("inputs"))
                       .def("__call__", nb::overload_cast<SignalArray<T> &>(&Array::bind_signal), nb::arg("signals"))
                       .def("__call__", nb::overload_cast<const SignalArrayView<T> &>(&Array::bind_signal), nb::arg("signals"))
                       .def("__call__", nb::overload_cast<InputArray<T> &>(&Array::bind_port), nb::arg("inputs"))
                       .def("__call__", nb::overload_cast<const InputArrayView<T> &>(&Array::bind_port), nb::arg("inputs"));
        bind_ndarray_common<Array>(cls, name);
        return cls;
    }

    template <typename T>
    static inline auto bind_output_array_view(nb::module_ &m, const char *name)
    {
        using View = OutputArrayView<T>;
        auto cls = nb::class_<View>(m, name)
                       .def("bind", nb::overload_cast<const SignalArrayView<T> &>(&View::bind_signal), nb::arg("signals"))
                       .def("bind", nb::overload_cast<const SignalArray<T> &>(&View::bind_signal), nb::arg("signals"))
                       .def("bind", nb::overload_cast<const OutputArrayView<T> &>(&View::bind_port), nb::arg("ports"))
                       .def("bind", nb::overload_cast<const OutputArray<T> &>(&View::bind_port), nb::arg("ports"))
                       .def("__call__", nb::overload_cast<const SignalArrayView<T> &>(&View::bind_signal), nb::arg("signals"))
                       .def("__call__", nb::overload_cast<const SignalArray<T> &>(&View::bind_signal), nb::arg("signals"))
                       .def("__call__", nb::overload_cast<const OutputArrayView<T> &>(&View::bind_port), nb::arg("ports"))
                       .def("__call__", nb::overload_cast<const OutputArray<T> &>(&View::bind_port), nb::arg("ports"));
        bind_ndarray_common<View>(cls, name);
        return cls;
    }

    template <typename T>
    static inline auto bind_output_array(nb::module_ &m, const char *name)
    {
        using Array = OutputArray<T>;
        auto cls = nb::class_<Array>(m, name)
                       .def(nb::init<const std::string &, Shape, int>(),
                            nb::arg("name"), nb::arg("shape"), nb::arg("width") = default_bitwidth<T>::value)
                       .def("bind", nb::overload_cast<SignalArray<T> &>(&Array::bind_signal), nb::arg("signals"))
                       .def("bind", nb::overload_cast<const SignalArrayView<T> &>(&Array::bind_signal), nb::arg("signals"))
                       .def("bind", nb::overload_cast<OutputArray<T> &>(&Array::bind_port), nb::arg("outputs"))
                       .def("bind", nb::overload_cast<const OutputArrayView<T> &>(&Array::bind_port), nb::arg("outputs"))
                       .def("__call__", nb::overload_cast<SignalArray<T> &>(&Array::bind_signal), nb::arg("signals"))
                       .def("__call__", nb::overload_cast<const SignalArrayView<T> &>(&Array::bind_signal), nb::arg("signals"))
                       .def("__call__", nb::overload_cast<OutputArray<T> &>(&Array::bind_port), nb::arg("outputs"))
                       .def("__call__", nb::overload_cast<const OutputArrayView<T> &>(&Array::bind_port), nb::arg("outputs"))
                       .def("__setitem__", [](Array &a, const Index &idx, const T &value)
                            { a.at(idx).write(value); }, nb::arg("index"), nb::arg("value"));
        bind_ndarray_common<Array>(cls, name);
        return cls;
    }

} // namespace dspsim::bindings
