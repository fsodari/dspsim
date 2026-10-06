#pragma once
#include <dspsim/clock.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;
    static inline auto bind_clock(nb::module_ &m, const char *name)
    {
        return nb::class_<Clock, Signal<uint8_t>>(m, name)
            .def(nb::new_(&Clock::create),
                 nb::arg("name"),
                 nb::arg("period"))
            .def_prop_ro("period", &Clock::period);
    }
} // namespace dspsim::bindings
