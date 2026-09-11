#include <dspsim/dspsim.h>
#include <dspsim/simulator.h>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/trampoline.h>

#include <iostream>

namespace nb = nanobind;
using namespace dspsim;

struct PyModel : public Model
{
    NB_TRAMPOLINE(Model);

    void eval_step() override
    {
        NB_OVERRIDE_PURE(eval_step);
    }

    void eval_end_step() override
    {
        NB_OVERRIDE(eval_end_step);
    }
};

NB_MODULE(_framework, m)
{
    m.doc() = "dspsim framework module";

    // Bind the Context class
    nb::class_<Context>(m, "Context")
        .def(nb::new_(&Context::obtain))
        // Properties
        .def_prop_ro("id", &Context::id)
        .def_prop_ro("models", &Context::models)
        .def_prop_ro("time", &Context::time)
        // Methods
        .def("register_model", &Context::register_model)
        .def("clear", &Context::clear)
        // Static Methods
        .def_static("obtain", &Context::obtain)
        .def_static("reset_global_context", &Context::reset_global_context);

    // Bind the Model class
    nb::class_<Model, PyModel>(m, "Model")
        .def(nb::init<>())
        // Properties
        .def_prop_ro("id", &Model::id)
        .def_prop_ro("context", &Model::context)
        // Methods
        .def("eval_step", &Model::eval_step)
        .def("eval_end_step", &Model::eval_end_step);

    nb::class_<Simulator>(m, "Simulator")
        .def(nb::init<ContextPtr, double, double>(), nb::arg("context"), nb::arg("time_unit") = 1e-9, nb::arg("time_precision") = 1e-9)
        // Properties
        .def_prop_ro("context", &Simulator::context)
        .def_prop_rw("time_unit", &Simulator::time_unit, &Simulator::set_time_unit)
        .def_prop_rw("time_precision", &Simulator::time_precision, &Simulator::set_time_precision)
        // Methods
        .def("set_timescale", &Simulator::set_timescale, nb::arg("time_unit"), nb::arg("time_precision"))
        .def("eval", &Simulator::eval)
        .def("run", &Simulator::run, nb::arg("time_step"));
}
