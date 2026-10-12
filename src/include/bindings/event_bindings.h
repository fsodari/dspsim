#pragma once
#include <dspsim/context.h>
#include <dspsim/event.h>
#include "nb_include.h"

#include <memory>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    static inline auto bind_sensitivity_event(nb::module_ &m, const char *name)
    {
        return nb::class_<SensitivityEvent>(m, name)
            // An event that Python creates belongs to the context, like a signal: the processes that wait
            // on it refer to it, so it must not go away with the last Python reference.
            .def(nb::new_([](Context &context)
                          {
                              auto event = std::make_shared<SensitivityEvent>(&context);
                              context.own_object(event);
                              return event; }),
                 nb::arg("context"))
            .def("notify", &SensitivityEvent::notify,
                 "The event occurs when the evaluation phase of the current delta cycle is over: every "
                 "process subscribed by then is evaluated in the next delta cycle. Between two runs: in the "
                 "next run.");
    }

} // namespace dspsim::bindings
