#pragma once
#include <dspsim/channel.h>
#include <dspsim/context.h>
#include "nb_include.h"
#include "python_reference.h"

#include <nanobind/trampoline.h>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    // A channel that is implemented in Python: the context's call of update() runs the method of the
    // Python subclass.
    class PyChannel : public Channel
    {
    public:
        NB_TRAMPOLINE(Channel);

        void update() override { NB_OVERRIDE_PURE(update); }
    };

    // request_update() is protected in C++, for the class that implements the channel. Python has no
    // protected members: the binding takes the address of the member through this class.
    class ChannelAccess : public Channel
    {
    public:
        using Channel::request_update;
    };

    static inline auto bind_channel(nb::module_ &m, const char *name)
    {
        return nb::class_<Channel, PyChannel>(
                   m, name,
                   "Base class for state that processes change during the evaluation phase and that becomes "
                   "visible to them in the update phase, the way the value of a signal does. A subclass calls "
                   "request_update() when it has something to commit and implements update().")
            /*
                The context keeps the Python object alive until it is cleared. A channel that asked for an
                update is on the context's stack, and nothing else guarantees that the object still exists when
                the update runs: a channel that is only a local variable would be collected first.
            */
            .def(
                "__init__", [](nb::pointer_and_handle<Channel> self, Context &context)
                {
                    new (static_cast<void *>(self.p)) PyChannel(&context);
                    context.own_object(python_reference(self.h)); },
                nb::arg("context"))
            .def_prop_ro("context", &Channel::context)
            .def("request_update", &ChannelAccess::request_update,
                 "Ask for update() to be called in the update phase of the current delta cycle, once, however "
                 "often it is requested. Between two runs: at the start of the next run.")
            // Only what a subclass that does not implement update() inherits, and what the stub documents:
            // the context calls the method of the Python subclass.
            .def(
                "update", [](Channel &)
                {
                    PyErr_SetString(PyExc_NotImplementedError, "A channel must implement update()");
                    throw nb::python_error(); },
                "Commit the changes made since the last update. Called by the context in the update phase. "
                "Must not write to a signal or request the update of a channel.");
    }

} // namespace dspsim::bindings
