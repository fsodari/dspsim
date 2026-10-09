#pragma once
#include <dspsim/modules/dff.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    template <typename T>
    static inline auto bind_dff_class(nb::module_ &m, const char *name)
    {
        return nb::class_<Dff<T>, Module>(m, name)
            .def(nb::new_([](const std::string &name)
                          { return Model::create<Dff<T>>(name); }),
                 nb::arg("name"))
            .def_ro("d", &Dff<T>::d)
            .def_ro("q", &Dff<T>::q);
    }

} // namespace dspsim::bindings
