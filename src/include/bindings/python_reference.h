#pragma once
#include "nb_include.h"

#include <memory>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    /*
        A reference to a Python object, for the context to hold with Context::own_object().

        An object that Python constructs (a Module or Channel subclass) is not a separate C++ allocation:
        its C++ part lives inside the Python object and is destroyed by Python when the last reference to
        the object goes. Owning it therefore means holding a reference, and the pointer returned here
        releases that reference where an ordinary shared pointer would delete.
    */
    static inline std::shared_ptr<void> python_reference(nb::handle object)
    {
        return std::shared_ptr<void>(object.inc_ref().ptr(), [](void *ptr)
                                     {
                                         // A context can be destroyed after the interpreter has shut down,
                                         // and from a thread that does not hold the GIL.
                                         if (nb::is_alive())
                                         {
                                             nb::gil_scoped_acquire gil;
                                             Py_DECREF(static_cast<PyObject *>(ptr));
                                         } });
    }

} // namespace dspsim::bindings
