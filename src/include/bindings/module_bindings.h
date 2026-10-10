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
          std::string name = module->hier_name() + "." + nb::cast<std::string>(nb::str(nb::getattr(task, "__name__", nb::str("task"))));
          return module->context()->_add_process(std::make_unique<PyTask>(module->context(), std::move(task), name));
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
              .def("wait", [](Module &module, uint64_t time_delta)
                   { return module.wait(time_delta); }, nb::arg("time_delta"))
              .def("wait", [](Module &module, SensitivityEvent &event)
                   { return module.wait(event); }, nb::arg("event"))
              .def("wait", [](Module &module, const std::vector<SensitivityEvent *> &events)
                   { return module.wait(events); }, nb::arg("events"))
              .def("wait", [](Module &module, SensitivityEvent &event, uint64_t timeout)
                   { return module.wait(event, timeout); }, nb::arg("event"), nb::arg("timeout"))
              .def("wait", [](Module &module, const std::vector<SensitivityEvent *> &events, uint64_t timeout)
                   { return module.wait(events, timeout); }, nb::arg("events"), nb::arg("timeout"))
              .def("ports", &Module::ports)
              .def("inputs", &Module::inputs)
              .def("outputs", &Module::outputs)
              .def("repr", &Module::repr)
              .def("__repr__", &Module::repr)
              .def("__str__", &Module::repr);
     }

} // namespace dspsim::bindings
