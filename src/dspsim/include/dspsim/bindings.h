#pragma once
#include "dspsim/dspsim.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/ndarray.h>
#include <nanobind/trampoline.h>

namespace dspsim
{
    namespace nb = nanobind;

    // Allows inheriting Model with a Python class.
    struct PyModel : public Model
    {
        NB_TRAMPOLINE(Model, 2);

        void eval_step() override
        {
            NB_OVERRIDE_PURE(eval_step);
        }
        void eval_end_step() override
        {
            NB_OVERRIDE(eval_end_step);
        }
    };

    // Bind context.
    static inline auto bind_context(nb::handle &scope, const char *name)
    {
        return nb::class_<Context>(scope, name)
            .def(nb::new_(&Context::create),
                 nb::arg("time_unit") = 1e-9, nb::arg("time_precision") = 1e-9)
            // Context manager functions
            .def("__enter__", [](ContextPtr context)
                 { return context; })
            .def("__exit__", [](ContextPtr context, nb::object exc_type, nb::object exc_value, nb::object traceback)
                 { context->clear(); }, nb::arg("exc_type") = nb::none(), nb::arg("exc_value") = nb::none(), nb::arg("traceback") = nb::none())
            // Timescale
            .def("set_timescale", &Context::set_timescale, nb::arg("time_unit"), nb::arg("time_precision"))
            .def_prop_rw("time_unit", &Context::time_unit, &Context::set_time_unit, nb::arg("time_unit"))
            .def_prop_rw("time_precision", &Context::time_precision, &Context::set_time_precision, nb::arg("time_precision"))
            .def_prop_ro("time_step", &Context::time_step)
            // global time
            .def("time", &Context::time)
            .def("clear", &Context::clear)
            .def("elaborate", &Context::elaborate)
            .def_prop_ro("elaborate_done", &Context::elaborate_done)
            .def("eval", &Context::eval)
            .def("advance", &Context::advance, nb::arg("time_inc") = 1)
            .def("own_model", &Context::own_model, nb::arg("model"))
            .def_prop_ro("models", &Context::models, nb::rv_policy::reference)
            .def("print_info", &Context::print_info);
    }

    // Bind global context.
    static inline auto _bind_context_factory(nb::handle &scope, const char *name)
    {
        return nb::class_<ContextFactory>(scope, name)
            .def("create", &ContextFactory::create)
            .def("obtain", &ContextFactory::obtain)
            .def("reset", &ContextFactory::reset);
    }
    static inline auto bind_global_context(nb::module_ &m)
    {
        m.def("link_context", &set_global_context_factory, nb::arg("global_context"));
        m.def("global_context", &get_global_context_factory, nb::rv_policy::reference);
    }

    // Bind Model.
    static inline auto bind_base_model(nb::handle &scope, const char *name)
    {
        return nb::class_<Model, PyModel>(scope, name)
            .def(nb::init<>())
            .def_prop_ro("context", &Model::context)
            .def("eval_step", &Model::eval_step)
            .def("eval_end_step", &Model::eval_end_step)
            .def_prop_ro_static("port_info", [](nb::handle _)
                                { return std::string(""); });
    }

    // Signals
    template <typename T>
    static inline auto bind_signal(nb::handle &scope, const char *name)
    {
        return nb::class_<Signal<T>>(scope, name)
            .def(nb::new_([](int initial)
                          { return create<Signal<T>>(initial); }),
                 nb::arg("initial") = 0)
            .def("posedge", &Signal<T>::posedge)
            .def("negedge", &Signal<T>::negedge)
            .def("changed", &Signal<T>::changed)
            .def_prop_rw(
                "d", &Signal<T>::_read_d, &Signal<T>::write, nb::arg("value"))
            .def_prop_ro("q", &Signal<T>::read);
    }

    template <typename T>
    static inline auto bind_dff(nb::handle &scope, const char *name)
    {
        return nb::class_<Dff<T>, Signal<T>>(scope, name)
            .def(nb::new_([](Signal<uint8_t> &clk, int initial)
                          { return create<Dff<T>>(clk, initial); }),
                 nb::arg("clk"),
                 nb::arg("initial") = 0);
    }

    // Bind Clock.
    static inline auto bind_clock(nb::handle &scope, const char *name)
    {
        return nb::class_<Clock, Signal<uint8_t>>(scope, name)
            .def(nb::new_([](double period)
                          { return create<Clock>(period); }),
                 nb::arg("period"))
            .def_prop_ro("period", &Clock::period);
    }

    // Bind AxisTx/Rx
    template <typename T>
    static inline auto bind_axis_tx(nb::handle &scope, const char *name)
    {
        return nb::class_<AxisTx<T>>(scope, name)
            .def(nb::new_(&AxisTx<T>::create),
                 nb::arg("clk"),
                 nb::arg("rst"),
                 nb::arg("m_axis_tdata"),
                 nb::arg("m_axis_tvalid"),
                 nb::arg("m_axis_tready"),
                 nb::arg("m_axis_tid") = nb::none(),
                 nb::arg("tid_pattern") = std::list<uint8_t>{0})
            .def("write", nb::overload_cast<T>(&AxisTx<T>::writei), nb::arg("data"))
            .def("write", nb::overload_cast<std::vector<T> &>(&AxisTx<T>::writev), nb::arg("data"))
            .def("write", nb::overload_cast<double, int>(&AxisTx<T>::write_convert_scalar), nb::arg("data"), nb::arg("q") = 0)
            .def("write", nb::overload_cast<std::vector<double> &, int>(&AxisTx<T>::write_convert_vector), nb::arg("data"), nb::arg("q") = 0);
    }
    template <typename T>
    static inline auto bind_axis_rx(nb::handle &scope, const char *name)
    {
        return nb::class_<AxisRx<T>>(scope, name)
            .def(nb::new_(&AxisRx<T>::create),
                 nb::arg("clk"),
                 nb::arg("rst"),
                 nb::arg("s_axis_tdata"),
                 nb::arg("s_axis_tvalid"),
                 nb::arg("s_axis_tready"),
                 nb::arg("s_axis_tid") = nb::none())
            .def_prop_rw("tready", &AxisRx<T>::get_tready, &AxisRx<T>::set_tready, nb::arg("value"))
            .def("read", &AxisRx<T>::read, nb::arg("clear") = true)
            .def("read_tid", &AxisRx<T>::read_tid, nb::arg("clear") = true);
    }
} // namespace dspsim
