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

    // static inline auto bind_module_context(nanobind::module_ &m)
    // {
    //     return m.def("get_context", []()
    //                  { return Context::context(); }, nanobind::sig("def get_context() -> dspsim.framework.Context"))
    //         .def("set_context", [](ContextPtr context)
    //              { Context::context(context); }, nanobind::sig("def set_context(context: dspsim.framework.Context) -> None"))
    //         .def("reset_context", []()
    //              { Context::context()->clear(); });
    // }

    // static inline auto bind_module_context(nanobind::module_ &m)
    // {

    //     m.def("get_context_factory", &get_global_context_factory);
    //     m.def("set_context_factory", &set_global_context_factory, nanobind::arg("context_factory"));
    // }

    // Bind context.
    static inline auto bind_context(nanobind::handle &scope, const char *name)
    {
        return nb::class_<Context>(scope, name)
            // .def(nanobind::init<int, int>(), nanobind::arg("time_unit") = 9, nanobind::arg("time_precision") = 9)
            .def(nb::new_([](double time_unit, double time_precision)
                          { return Context::create(time_unit, time_precision); }),
                 nb::arg("time_unit") = 1e-9, nb::arg("time_precision") = 1e-9)

            .def("__enter__", [](ContextPtr context)
                 { return context; })
            .def("__exit__", [](ContextPtr context, nb::object exc_type, nb::object exc_value, nb::object traceback)
                 { context->clear(); }, nb::arg("exc_type") = nb::none(), nb::arg("exc_value") = nb::none(), nb::arg("traceback") = nb::none())
            .def("set_timescale", &Context::set_timescale, nanobind::arg("time_unit"), nanobind::arg("time_precision"))
            .def_prop_rw("time_unit", &Context::time_unit, &Context::set_time_unit, nanobind::arg("time_unit"))
            .def_prop_rw("time_precision", &Context::time_precision, &Context::set_time_precision, nanobind::arg("time_precision"))
            .def_prop_ro("time_step", &Context::time_step)
            .def("time", &Context::time)
            .def("clear", &Context::clear)
            .def("elaborate", &Context::elaborate)
            .def_prop_ro("elaborate_done", &Context::elaborate_done)
            .def("eval", &Context::eval)
            .def("advance", &Context::advance, nanobind::arg("time_inc") = 1)
            .def("own_model", &Context::own_model, nanobind::arg("model"))
            .def_prop_ro("models", &Context::models, nanobind::rv_policy::reference)
            .def("print_info", &Context::print_info);
    }

    // Bind Model.
    static inline auto bind_base_model(nanobind::handle &scope, const char *name)
    {
        return nanobind::class_<Model, PyModel>(scope, name)
            .def(nanobind::init<>())
            .def_prop_ro("context", &Model::context)
            .def("eval_step", &Model::eval_step)
            .def("eval_end_step", &Model::eval_end_step)
            .def_prop_ro_static("port_info", [](nanobind::handle _)
                                { return std::string(""); });
    }

    // Signals
    template <typename T>
    static inline auto bind_signal(nanobind::handle &scope, const char *name)
    {
        return nanobind::class_<Signal<T>>(scope, name)
            // .def(nanobind::init<T>(), nanobind::arg("initial") = 0)
            .def(nanobind::new_([](int initial)
                                { return create<Signal<T>>(initial); }),
                 nanobind::arg("initial") = 0)
            .def("posedge", &Signal<T>::posedge)
            .def("negedge", &Signal<T>::negedge)
            .def("changed", &Signal<T>::changed)
            .def_prop_rw(
                "d", &Signal<T>::_read_d, &Signal<T>::write, nanobind::arg("value"))
            .def_prop_ro("q", &Signal<T>::read);
        // .def_static("array", [](size_t n)
        //             { return Signal<T>::new_array(n); }, nanobind::arg("n"));
    }
    template <typename T>
    static inline auto bind_signal_array(nanobind::handle &scope, const char *name)
    {
        // return nanobind::class_<Signal<T>[]>(scope, name)
        //     .def(nanobind::new_([](int n)
        //                         { return Signal<T>::new_array(n); }));
    }

    template <typename T>
    static inline auto bind_dff(nanobind::handle &scope, const char *name)
    {
        return nanobind::class_<Dff<T>, Signal<T>>(scope, name)
            .def(nanobind::new_([](Signal<uint8_t> &clk, int initial)
                                { return create<Dff<T>>(clk, initial); }),
                 nanobind::arg("clk"),
                 nanobind::arg("initial") = 0);
    }

    // Bind Clock.
    static inline auto bind_clock(nanobind::handle &scope, const char *name)
    {
        return nanobind::class_<Clock, Signal<uint8_t>>(scope, name)
            .def(nanobind::new_([](double period)
                                { return create<Clock>(period); }),
                 nanobind::arg("period"))
            .def_prop_ro("period", &Clock::period);
    }
} // namespace dspsim
