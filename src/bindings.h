#pragma once

#include <dspsim/dspsim.h>
#include <dspsim/modules/dff.h>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/function.h>
#include <nanobind/trampoline.h>

// bindings is only included by _framework.cpp so this shouldn't pollute the namespace.
namespace nb = nanobind;

/**
 * Make the SensitivityEvent type opaque to nanobind.
 * This prevents nanobind from trying to automatically wrap the SensitivityEvent type as an immutable list.
 */
NB_MAKE_OPAQUE(dspsim::SensitivityEvent);

namespace dspsim
{
    static inline auto bind_context(nb::module_ &m, const char *name)
    {
        // Bind the Context class
        return nb::class_<Context>(m, name)
            .def(nb::new_(&Context::create), nb::arg("name") = "")
            // Methods
            .def("clear", &Context::clear)
            .def("elaborate", &Context::elaborate)
            .def("eval", &Context::eval)
            .def("run", &Context::run, nb::arg("time_inc") = 0)
            .def("print_hierarchy", &Context::print_hierarchy, nb::arg("parent").none() = nullptr, nb::arg("depth") = 0)
            .def("children", &Context::children, nb::arg("parent").none())

            // Properties
            .def_prop_ro("name", &Context::name)
            .def_prop_ro("id", &Context::id)
            .def_prop_ro("models", &Context::models)
            .def_prop_ro("modules", &Context::modules)
            .def_prop_ro("signals", &Context::signals)

            .def_prop_ro("time", &Context::time)
            .def_prop_rw("time_unit", &Context::time_unit, &Context::set_time_unit)
            .def_prop_rw("log_level", &Context::log_level, &Context::set_log_level)

            .def("__repr__", &Context::repr)
            .def("__str__", &Context::repr)

            // Python module base class will need to explicitly call this.
            .def("own_model", &Context::_own_model)
            // .def("own_module", &Context::_own_module, nb::arg("module"))

            // Register a process.
            .def("register_process", &Context::register_process_func,
                 nb::arg("func"),
                 nb::arg("source"),
                 nb::arg("name") = "",
                 nb::rv_policy::reference)

            // Static Methods
            .def_static("obtain", &Context::obtain)
            .def_static("reset", &Context::reset)
            .def_static("create", &Context::create, nb::arg("name") = "");
    }

    static inline auto bind_context_factory(nb::module_ &m, const char *name)
    {
        m.def("set_global_context_factory", &set_global_context_factory, nb::arg("context_factory"));
        m.def("get_global_context_factory", &get_global_context_factory);
        m.def("reset_global_context_factory", &reset_global_context_factory);

        return nb::class_<ContextFactory>(m, name)
            .def("obtain", &ContextFactory::obtain)
            .def("reset", &ContextFactory::reset)
            .def("create", &ContextFactory::create, nb::arg("name") = "");
    }

    static inline auto bind_model(nb::module_ &m, const char *name)
    {
        // Bind the Model class
        return nb::class_<Model>(m, name)
            .def(nb::init<const std::string &, const std::string &>(),
                 nb::arg("name"),
                 nb::arg("kind") = "model")
            // Methods.
            .def("finalize", &Model::finalize)
            // Properties
            .def_prop_ro("context", &Model::context)
            .def_prop_ro("name", &Model::name)
            .def_prop_ro("id", &Model::id)
            .def_prop_ro("kind", &Model::kind)
            .def_prop_ro("hier_name", &Model::hier_name)
            .def_prop_ro("parent", &Model::parent)

            .def("repr", &Model::repr)
            .def("__repr__", &Model::repr)
            .def("__str__", &Model::repr);
    }

    static inline Process *_process_always_func(Process *self, nb::args args)
    {
        for (auto arg : args)
        {
            if (nb::isinstance<SensitivityEvent>(arg))
            {
                self->always(&nb::cast<SensitivityEvent &>(arg));
            }
            else if (nb::isinstance<InputBase>(arg))
            {
                self->always(nb::cast<InputBase &>(arg)._change());
            }
            else if (nb::isinstance<SignalBase>(arg))
            {
                self->always(nb::cast<SignalBase &>(arg)._change());
            }
            else
            {
                throw std::runtime_error("Unsupported argument type for SensitivityList");
            }
        }
        return self;
    }

    static inline auto bind_process(nb::module_ &m, const char *name)
    {
        return nb::class_<Process>(m, name)
            .def_prop_ro("name", &Process::name)
            .def_prop_ro("id", &Process::id)
            .def_prop_ro("source", &Process::source)
            .def("initialize", &Process::_set_initialize, nb::arg("init"), nb::rv_policy::reference_internal)
            .def("always", &Process::_always_str, nb::arg("event_name"), nb::rv_policy::reference_internal)
            .def("always", &_process_always_func, nb::rv_policy::reference_internal, nb::sig("def always(self, *args: SensitivityEvent | InputBase | SignalBase) -> Process"));
    }

    static inline auto bind_time_event(nb::module_ &m, const char *name)
    {
        return nb::class_<TimeEvent>(m, name);
    }

    static inline auto bind_sensitivity_event(nb::module_ &m, const char *name)
    {
        return nb::class_<SensitivityEvent>(m, name);
    }

    static inline auto bind_signal_base(nb::module_ &m, const char *name)
    {
        return nb::class_<SignalBase, Model>(m, name)
            .def("pos", &SignalBase::pos, nb::rv_policy::reference_internal)
            .def("neg", &SignalBase::neg, nb::rv_policy::reference_internal)
            .def("change", &SignalBase::operator SensitivityEvent *, nb::rv_policy::reference_internal)
            .def("posedge", &SignalBase::posedge)
            .def("negedge", &SignalBase::negedge)
            .def("changed", &SignalBase::changed);
    }

    template <typename T>
    static inline auto bind_signal_class(nb::module_ &m, const char *name)
    {
        return nb::class_<Signal<T>, SignalBase>(m, name)
            .def(nb::new_(&Signal<T>::create),
                 nb::arg("name"),
                 nb::arg("width") = default_bitwidth<T>::value,
                 nb::arg("init") = 0,
                 nb::arg("is_signed") = false)
            // Methods
            .def("write", &Signal<T>::write, nb::arg("value"))
            .def("read", &Signal<T>::read)
            // Properties
            .def_prop_ro("width", &Signal<T>::width)
            .def_prop_ro("is_signed", &Signal<T>::is_signed)
            .def_prop_rw("value", &Signal<T>::read, &Signal<T>::write, nb::arg("value"))
            .def_prop_rw("d", &Signal<T>::_read_d, &Signal<T>::write, nb::arg("value"))
            .def_prop_ro("q", &Signal<T>::read);
    }

    static inline auto bind_input_base(nb::module_ &m, const char *name)
    {
        return nb::class_<InputBase, Model>(m, name)
            .def("pos", &InputBase::pos, nb::rv_policy::reference_internal)
            .def("neg", &InputBase::neg, nb::rv_policy::reference_internal);
    }

    template <typename T>
    static inline auto bind_input(nb::module_ &m, const char *name)
    {
        return nb::class_<Input<T>, InputBase>(m, name)
            // Don't need to "create". Ports will always exist inside a module.
            .def(nb::init<const std::string &>(), nb::arg("name"))
            // Methods
            .def("bind", &Input<T>::_bind_signal, nb::arg("signal"))
            .def("bind", &Input<T>::_bind_port, nb::arg("input"))
            .def("__call__", &Input<T>::_bind_signal, nb::arg("signal"))
            .def("__call__", &Input<T>::_bind_port, nb::arg("input"))
            .def("read", &Input<T>::read)
            .def_prop_ro("value", &Input<T>::read)
            .def_prop_ro("q", &Input<T>::read)
            .def("posedge", &Input<T>::posedge)
            .def("negedge", &Input<T>::negedge)
            .def("changed", &Input<T>::changed);
    }

    template <typename T>
    static inline auto bind_output(nb::module_ &m, const char *name)
    {
        return nb::class_<Output<T>, Model>(m, name)
            // Don't need to "create". Ports will always exist inside a module.
            .def(nb::init<const std::string &>(), nb::arg("name"))
            // Methods
            .def("bind", &Output<T>::_bind_signal, nb::arg("signal"))
            .def("bind", &Output<T>::_bind_port, nb::arg("output"))
            .def("__call__", &Output<T>::_bind_signal, nb::arg("signal"))
            .def("__call__", &Output<T>::_bind_port, nb::arg("output"))
            .def("write", &Output<T>::write, nb::arg("value"))
            .def_prop_rw("value", &Output<T>::read, &Output<T>::write, nb::arg("value"))
            .def_prop_rw("d", &Output<T>::_read_d, &Output<T>::write, nb::arg("value"))
            .def_prop_ro("q", &Output<T>::read);
    }

    static inline auto bind_module_name(nb::module_ &m, const char *name)
    {
        return nb::class_<ModuleName>(m, name)
            .def(nb::init<const std::string &>(), nb::arg("name"))
            .def_prop_ro("name", &ModuleName::name);
    }

    static inline Process *_module_process_helper(Module *module, std::function<void()> func, const std::string &name = "")
    {
        return module->context()->register_process_func(func, module, name);
    }

    static inline auto bind_module(nb::module_ &m, const char *name)
    {
        return nb::class_<Module, Model>(m, name)
            .def(nb::init<ModuleName &>(), nb::arg("name"))
            // Methods.
            .def("finalize", &Module::finalize)
            .def("process", &_module_process_helper, nb::arg("func"), nb::arg("name") = "", nb::rv_policy::reference_internal)
            .def("ports", &Module::ports)
            .def("inputs", &Module::inputs)
            .def("outputs", &Module::outputs)
            .def("repr", &Module::repr)
            .def("__repr__", &Module::repr)
            .def("__str__", &Module::repr);
    }

    static inline auto bind_clock(nb::module_ &m, const char *name)
    {
        return nb::class_<Clock, Signal<uint8_t>>(m, name)
            .def(nb::new_(&Clock::create),
                 nb::arg("name"),
                 nb::arg("period"))
            // .def_prop_rw("name", &Clock::name, &Clock::set_name)
            .def_prop_ro("period", &Clock::period);
    }

    template <typename T>
    static inline auto bind_dff_class(nb::module_ &m, const char *name)
    {
        return nb::class_<Dff<T>, Module>(m, name)
            .def(nb::new_([](const std::string &name)
                          { return Model::create<Dff<T>>(name); }),
                 nb::arg("name"))
            .def_ro("d", &Dff<T>::d)
            .def_ro("q", &Dff<T>::q);
    }
}