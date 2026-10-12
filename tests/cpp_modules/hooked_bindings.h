#pragma once
#include "Hooked.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include <vector>

/// The bindings of Hooked that the builder cannot generate. The generated bindings call this before they bind
/// the constructors and methods. M is the class that Python has: a class of the build, derived from
/// demo::Hooked.
template <typename M>
void dspsim_bind(nanobind::module_ &, nanobind::class_<M, dspsim::Module> &cls)
{
    namespace nb = nanobind;

    // The classes that the constructors and methods use, as members of the Python class: Hooked.Config.
    // A C++ class has one Python class in a process: bind them unless another extension module has.
    if (!nb::type<demo::Config>().is_valid())
    {
        nb::class_<demo::Config>(cls, "Config")
            .def(nb::init<>())
            .def_rw("gain", &demo::Config::gain)
            .def_rw("offset", &demo::Config::offset);
        nb::enum_<demo::Mode>(cls, "Mode")
            .value("Bypass", demo::Mode::Bypass)
            .value("Scale", demo::Mode::Scale);
    }
    else
    {
        cls.attr("Config") = nb::type<demo::Config>();
        cls.attr("Mode") = nb::type<demo::Mode>();
    }

    // A method in a form that Python can call. A function that takes the module takes an M.
    cls.def("fill", [](M &self, int n)
            {
                std::vector<int> values(n);
                self.fill(values.data(), n);
                return values; }, nb::arg("n"));
    // A member function of the module's class is bound as it is.
    cls.def("apply_again", &demo::Hooked::apply, nb::arg("value"));
}
