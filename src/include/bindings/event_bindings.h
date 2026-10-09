#pragma once
#include <dspsim/event.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;

    static inline auto bind_time_event(nb::module_ &m, const char *name)
    {
        return nb::class_<TimeEvent>(m, name);
    }

    static inline auto bind_sensitivity_event(nb::module_ &m, const char *name)
    {
        return nb::class_<SensitivityEvent>(m, name);
    }

} // namespace dspsim::bindings
