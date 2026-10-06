#pragma once
#include <dspsim/model.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    static inline auto bind_model(nb::module_ &m, const char *name)
    {
        // Bind the Model class
        return nb::class_<Model>(m, name)
            .def(nb::init<const std::string &, const std::string &>(),
                 nb::arg("name"),
                 nb::arg("kind") = "model")
            // Methods.
            .def("finalize", &Model::finalize)
            // Properties
            .def_prop_ro("context", &Model::context)
            .def_prop_ro("name", &Model::name)
            .def_prop_ro("id", &Model::id)
            .def_prop_ro("kind", &Model::kind)
            .def_prop_ro("hier_name", &Model::hier_name)
            .def_prop_ro("parent", &Model::parent)

            .def("repr", &Model::repr)
            .def("__repr__", &Model::repr)
            .def("__str__", &Model::repr);
    }

} // namespace dspsim::bindings
