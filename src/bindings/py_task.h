#pragma once
#include <dspsim/process.h>
#include "nb_include.h"

namespace dspsim::bindings
{
    namespace nb = nanobind;
    // Drives a native Python coroutine object (e.g. from `async def`) one step at
    // a time, matching CppTask's resume-until-suspend behavior. Uses the raw
    // PyIter_Send() C API instead of calling the `.send()` method and catching
    // StopIteration: for a genuine coroutine object this dispatches straight to
    // CPython's internal generator-send routine, which reports both "yielded"
    // and "returned" via a plain status code, with no Python exception raised
    // for the (extremely common) non-error cases.
    class PyTask : public ProcessBase
    {
    public:
        explicit PyTask(nb::object coro) : coro_(std::move(coro)) {}

        bool done() const override { return done_; }

        void resume() override
        {
            if (done_)
                return;
            PyObject *result = nullptr;
            PySendResult status = PyIter_Send(coro_.ptr(), Py_None, &result);
            switch (status)
            {
            case PYGEN_NEXT:
                Py_XDECREF(result);
                break;
            case PYGEN_RETURN:
                Py_XDECREF(result);
                done_ = true;
                break;
            case PYGEN_ERROR:
            default:
                done_ = true;
                throw nb::python_error();
            }
        }

    private:
        nb::object coro_;
        bool done_ = false;
    };
    static inline auto bind_py_task(nb::module_ &m, const char *name)
    {
        return nb::class_<PyTask, ProcessBase>(m, name);
    }

} // namespace dspsim::bindings
