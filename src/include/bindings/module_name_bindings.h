#pragma once
#include <dspsim/module_name.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;
    static inline auto bind_module_name(nb::module_ &m, const char *name)
    {
        return nb::class_<ModuleName>(m, name)
            // .def(nb::init<const std::string &>(), nb::arg("name"))
            .def(nb::init_implicit<const std::string &>())
            .def_prop_ro("name", &ModuleName::name);
    }
} // namespace dspsim::bindings
