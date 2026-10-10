#pragma once
#include <dspsim/signal.h>
#include "nb_include.h"
#include "ndarray_bindings.h"
#include "bitsel_bindings.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;
    static inline auto bind_signal_base(nb::module_ &m, const char *name)
    {
        return nb::class_<SignalBase, Model>(m, name)
            .def("change", &SignalBase::operator SensitivityEvent &, nb::rv_policy::reference_internal)
            .def("pos", &SignalBase::pos, nb::rv_policy::reference_internal)
            .def("neg", &SignalBase::neg, nb::rv_policy::reference_internal)
            .def_prop_ro("width", &SignalBase::width)
            // Bit selections. sig[hi:lo] is inclusive, as in SystemVerilog. Selections are unsigned.
            .def("slice", &SignalBase::slice, nb::arg("hi"), nb::arg("lo"))
            .def("__getitem__", [](SignalBase &s, nb::handle key)
                 {
                     auto range = resolve_bit_key(key, s.width());
                     return s.slice(range.hi, range.lo); }, nb::arg("key"), nb::sig("def __getitem__(self, key: int | slice, /) -> BitSel"));
    }

    template <typename T>
    static inline auto bind_signal_class(nb::module_ &m, const char *name)
    {
        return nb::class_<Signal<T>, SignalBase>(m, name)
            .def(nb::new_(&Signal<T>::create),
                 nb::arg("name"),
                 nb::arg("width") = default_bitwidth<T>::value,
                 nb::arg("init") = 0)
            // Methods
            .def("write", &Signal<T>::write, nb::arg("value"))
            .def("read", &Signal<T>::read)
            // Properties
            .def_prop_ro("width", &Signal<T>::width)
            .def_prop_ro("is_signed", [](const Signal<T> &)
                         { return std::is_signed_v<T>; })
            .def_prop_rw("value", &Signal<T>::read, &Signal<T>::write, nb::arg("value"))
            .def_prop_rw("d", &Signal<T>::read_d_, &Signal<T>::write, nb::arg("value"))
            .def_prop_ro("q", &Signal<T>::read);
    }

    template <typename T>
    static inline auto bind_signal_array_view(nb::module_ &m, const char *name)
    {
        using View = SignalArrayView<T>;
        auto cls = nb::class_<View>(m, name)
                       .def("__setitem__", [](View &a, const Index &idx, const T &value)
                            { a.at(idx).write(value); }, nb::arg("index"), nb::arg("value"));
        bind_ndarray_common<View>(cls, name);
        return cls;
    }

    template <typename T>
    static inline auto bind_signal_array(nb::module_ &m, const char *name)
    {
        using Array = SignalArray<T>;
        auto cls = nb::class_<Array>(m, name)
                       .def(nb::new_(&Array::create),
                            nb::arg("name"),
                            nb::arg("shape"),
                            nb::arg("width") = default_bitwidth<T>::value,
                            nb::arg("init") = 0)
                       .def("__setitem__", [](Array &a, const Index &idx, const T &value)
                            { a.at(idx).write(value); }, nb::arg("index"), nb::arg("value"));
        bind_ndarray_common<Array>(cls, name);
        return cls;
    }

} // namespace dspsim::bindings
