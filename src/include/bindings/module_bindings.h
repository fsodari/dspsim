#pragma once
#include <dspsim/module.h>
#include "py_task.h"
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;
    static inline auto _module_process_helper(Module *module, std::function<void()> func, const std::string &name = "")
    {
        return module->context()->register_process_func(func, name);
    }
    static inline auto _module_coro_helper(Module *module, nb::object task)
    {
        module->context()->_processes.push_back(std::make_unique<PyTask>(module->context(), std::move(task)));
        return module->context()->_processes.back().get();
    }

    static inline auto bind_module(nb::module_ &m, const char *name)
    {
        return nb::class_<Module, Model>(m, name)
            .def(nb::init<ModuleName &>(), nb::arg("name"))
            // Methods.
            .def("finalize", &Module::finalize)
            .def("process", &_module_process_helper, nb::arg("func"), nb::arg("name") = "", nb::rv_policy::reference)
            .def("add_task", &_module_coro_helper, nb::arg("task"), nb::rv_policy::reference)
            .def("wait", [](Module &module)
                 { return module.wait(); })
            .def("wait", [](Module &module, uint64_t time_delta, ProcessBase *process)
                 { return module.wait(time_delta, process); }, nb::arg("time_delta"), nb::arg("process") = nullptr)
            .def("wait", [](Module &module, SensitivityEvent &event, ProcessBase *process)
                 { return module.wait(event, process); }, nb::arg("event"), nb::arg("process") = nullptr)
            .def("wait", [](Module &module, const std::vector<SensitivityEvent *> &events, ProcessBase *process)
                 { return module.wait(events, process); }, nb::arg("events"), nb::arg("process") = nullptr)
            .def("ports", &Module::ports)
            .def("inputs", &Module::inputs)
            .def("outputs", &Module::outputs)
            .def("repr", &Module::repr)
            .def("__repr__", &Module::repr)
            .def("__str__", &Module::repr);
    }

} // namespace dspsim::bindings
