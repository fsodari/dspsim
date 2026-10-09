#pragma once
#include <dspsim/vmodule/vport.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;
    template <typename T>
    static inline auto bind_vinput(nb::module_ &m, const char *name)
    {
        return nb::class_<VInput<T>, Input<T>>(m, name);
    }
    template <typename T>
    static inline auto bind_voutput(nb::module_ &m, const char *name)
    {
        return nb::class_<VOutput<T>, Output<T>>(m, name);
    }

    // Verilated array ports are only constructed by generated C++ modules, so they have no python constructor.
    template <typename T>
    static inline auto bind_vinput_array(nb::module_ &m, const char *name)
    {
        return nb::class_<VInputArray<T>, InputArray<T>>(m, name);
    }
    template <typename T>
    static inline auto bind_voutput_array(nb::module_ &m, const char *name)
    {
        return nb::class_<VOutputArray<T>, OutputArray<T>>(m, name);
    }

} // namespace dspsim::bindings
