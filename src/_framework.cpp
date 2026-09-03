#include <dspsim/dspsim.h>

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
        NB_OVERRIDE(eval_step);
    }

    void eval_end_step() override
    {
        NB_OVERRIDE(eval_end_step);
    }
};

class AModel : public Model
{
public:
    void eval_step() override
    {
        std::cout << "AModel eval_step called" << std::endl;
    }
    void eval_end_step() override
    {
        std::cout << "AModel eval_end_step called" << std::endl;
    }
};

NB_MODULE(_framework, m)
{
    m.doc() = "dspsim framework module";

    // Bind the Context class
    nb::class_<Context>(m, "Context")
        .def(nb::new_(&Context::obtain))
        .def_static("reset", &Context::reset)
        .def_static("obtain", &Context::obtain)
        .def("clear", &Context::clear)
        .def("register_model", &Context::register_model)
        .def("elaborate", &Context::elaborate)
        .def("eval", &Context::eval)
        .def_prop_ro("id", &Context::id)
        .def_prop_ro("models", &Context::models)
        .def("__enter__", [](ContextPtr self)
             { return self; })
        .def("__exit__", [](ContextPtr self, nb::handle, nb::handle, nb::handle)
             { self->clear(); }, nb::arg().none(), nb::arg().none(), nb::arg().none());

    // Bind the Model class
    nb::class_<Model, PyModel>(m, "Model")
        // .def(nb::new_(&Model::create<Model>))
        .def(nb::init<>())
        .def_prop_ro("id", &Model::id)
        .def_prop_ro("context", &Model::context);
}
