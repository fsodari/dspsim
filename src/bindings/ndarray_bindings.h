#pragma once
#include <dspsim/ndarray.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    // Members shared by every array class. Shapes and indices are python tuples.
    template <typename Array, typename NbClass>
    static inline void bind_ndarray_common(NbClass &cls)
    {
        cls.def_prop_ro("shape", [](const Array &a)
                        { return nb::tuple(nb::cast(a.shape())); })
            .def_prop_ro("ndim", [](const Array &a)
                        { return a.ndim(); })
            .def("extent", [](const Array &a, std::size_t dim)
                 { return a.extent(dim); }, nb::arg("dim"))
            .def("__len__", [](const Array &a)
                 { return a.size(); })
            .def_prop_ro("size", [](const Array &a)
                        { return a.size(); })
            .def("flat", [](Array &a, std::size_t i) -> auto &
                 { return a.flat(i); }, nb::arg("index"), nb::rv_policy::reference_internal)
            .def("at", [](Array &a, const Index &idx) -> auto &
                 { return a.at(idx); }, nb::arg("index"), nb::rv_policy::reference_internal)
            .def("__getitem__", [](Array &a, const Index &idx) -> auto &
                 { return a.at(idx); }, nb::arg("index"), nb::rv_policy::reference_internal);
    }

} // namespace dspsim::bindings
