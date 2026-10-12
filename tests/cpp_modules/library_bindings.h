#pragma once
#include "Hooked.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <type_traits>
#include <vector>

/// The bindings header of a library ([tool.dspsim.cpp] bindings). The C++ modules of a library share one
/// header, and dspsim_bind() is called for each of them: M tells which.
template <typename M>
void dspsim_bind(nanobind::module_ &m, nanobind::class_<M, dspsim::Module> &cls)
{
    namespace nb = nanobind;

    if constexpr (std::is_base_of_v<demo::Hooked, M>)
    {
        // The classes that Hooked uses, in the library module: library.Config. A C++ class has one Python
        // class in a process, so bind them unless another extension module has, and use its classes if so.
        if (!nb::type<demo::Config>().is_valid())
        {
            nb::class_<demo::Config>(m, "Config")
                .def(nb::init<>())
                .def_rw("gain", &demo::Config::gain)
                .def_rw("offset", &demo::Config::offset);
            nb::enum_<demo::Mode>(m, "Mode")
                .value("Bypass", demo::Mode::Bypass)
                .value("Scale", demo::Mode::Scale);
        }
        else
        {
            m.attr("Config") = nb::type<demo::Config>();
            m.attr("Mode") = nb::type<demo::Mode>();
        }

        cls.def("fill", [](M &self, int n)
                {
                    std::vector<int> values(n);
                    self.fill(values.data(), n);
                    return values; }, nb::arg("n"));
    }
}
