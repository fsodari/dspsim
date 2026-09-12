#include <dspsim/dspsim.h>
#include <dspsim/signal.h>
#include <dspsim/dff.h>
#include <dspsim/port.h>
#include <dspsim/clock.h>
#include <dspsim/vmodel.h>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/trampoline.h>

#include <iostream>
#include <cstdint>

#include <VSimpleModel.h>
// #include <verilated_vcd_c.h>
#include <verilated_fst_c.h>

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

    const std::string repr() const override
    {
        NB_OVERRIDE(repr);
    }
};

template <typename T>
auto bind_signal_class(nb::module_ &m, const char *name)
{
    return nb::class_<Signal<T>, Model>(m, name)
        .def(nb::new_(&Signal<T>::create), nb::arg("width") = default_bitwidth<T>::value, nb::arg("init") = 0, nb::arg("is_signed") = false)
        // Properties
        .def_prop_ro("width", &Signal<T>::width)
        .def_prop_ro("is_signed", &Signal<T>::is_signed)
        .def_prop_rw("d", &Signal<T>::d, &Signal<T>::set_d)
        .def_prop_ro("q", &Signal<T>::q);
}
template <typename T>
auto bind_dff_class(nb::module_ &m, const char *name)
{
    return nb::class_<Dff<T>, Signal<T>>(m, name)
        .def(nb::new_(&Dff<T>::create), nb::arg("clk"), nb::arg("width") = default_bitwidth<T>::value, nb::arg("init") = 0, nb::arg("is_signed") = false)
        // Properties
        .def_prop_ro("width", &Dff<T>::width)
        .def_prop_ro("is_signed", &Dff<T>::is_signed)
        .def_prop_rw("d", &Dff<T>::d, &Dff<T>::set_d)
        .def_prop_ro("q", &Dff<T>::q);
}

class SimpleModel : public VModel<VSimpleModel, VerilatedFstC>
{
protected:
    InputPtr<uint8_t> _clk;
    InputPtr<uint8_t> _rst;
    InputPtr<uint8_t> _i;
    OutputPtr<uint8_t> _o;

public:
    SimpleModel(
        SignalPtr<uint8_t> clk,
        SignalPtr<uint8_t> rst,
        SignalPtr<uint8_t> i,
        SignalPtr<uint8_t> o)
    {
        _clk = Model::create<Input<uint8_t>>(clk, top->clk);
        _rst = Model::create<Input<uint8_t>>(rst, top->rst);
        _i = Model::create<Input<uint8_t>>(i, top->i);
        _o = Model::create<Output<uint8_t>>(o, top->o);

        _clk->set_parent_id(this->id());
        _rst->set_parent_id(this->id());
        _i->set_parent_id(this->id());
        _o->set_parent_id(this->id());
    }

    static auto create(
        SignalPtr<uint8_t> clk,
        SignalPtr<uint8_t> rst,
        SignalPtr<uint8_t> i,
        SignalPtr<uint8_t> o)
    {
        return Model::create<SimpleModel>(clk, rst, i, o);
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
        .def_prop_ro("time_unit", &Context::time_unit)
        .def_prop_ro("time_precision", &Context::time_precision)
        .def("set_timescale", &Context::set_timescale, nb::arg("time_unit"), nb::arg("time_precision"))

        .def("__repr__", &Context::repr)
        .def("__str__", &Context::repr)

        // Methods
        .def("register_model", &Context::register_model)
        .def("clear", &Context::clear)
        .def("eval", &Context::eval)
        .def("run", &Context::run, nb::arg("time_inc"))
        // Static Methods
        .def_static("obtain", &Context::obtain)
        .def_static("reset_global_context", &Context::reset_global_context);

    // Bind the Model class
    nb::class_<Model, PyModel>(m, "Model")
        .def(nb::init<const std::string &>(), nb::arg("kind") = "model")
        // Properties
        .def_prop_ro("context", &Model::context)
        .def_prop_ro("id", &Model::id)
        .def_prop_ro("kind", &Model::kind)
        .def_prop_rw("parent_id", &Model::parent_id, &Model::set_parent_id)

        .def("__repr__", &Model::repr)
        .def("__str__", &Model::repr)

        // Methods
        .def("eval_step", &Model::eval_step)
        .def("eval_end_step", &Model::eval_end_step);

    // Bind Signals
    bind_signal_class<uint8_t>(m, "Signal8");
    bind_signal_class<uint16_t>(m, "Signal16");
    bind_signal_class<uint32_t>(m, "Signal32");
    bind_signal_class<uint64_t>(m, "Signal64");

    // Clock
    nb::class_<Clock, Signal<uint8_t>>(m, "Clock")
        .def(nb::new_(&Clock::create), nb::arg("period"))
        .def_prop_ro("period", &Clock::period)
        .def_prop_ro("posedge", &Clock::posedge);

    // Dff
    bind_dff_class<uint8_t>(m, "Dff8");
    bind_dff_class<uint16_t>(m, "Dff16");
    bind_dff_class<uint32_t>(m, "Dff32");
    bind_dff_class<uint64_t>(m, "Dff64");

    nb::class_<SimpleModel, Model>(m, "SimpleModel")
        .def(nb::new_(&SimpleModel::create), nb::arg("clk"), nb::arg("rst"), nb::arg("i"), nb::arg("o"))
        .def("trace", &SimpleModel::trace, nb::arg("trace_path"), nb::arg("levels") = 99, nb::arg("options") = 0)
        .def("close", &SimpleModel::close);
}
