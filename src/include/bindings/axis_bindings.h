#pragma once
#include <dspsim/modules/axis_rx.h>
#include <dspsim/modules/axis_tx.h>
#include "coro_bindings.h"
#include "nb_include.h"

#include <vector>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    template <typename T>
    static inline auto bind_axis_rx(nb::module_ &m, const char *name)
    {
        using Rx = AxisRx<T>;
        return nb::class_<Rx, Module>(m, name)
            .def(nb::new_([](const std::string &name)
                          { return Model::create<Rx>(name); }),
                 nb::arg("name"))
            .def_ro("clk", &Rx::clk)
            .def_ro("rst", &Rx::rst)
            .def_ro("s_axis_tdata", &Rx::s_axis_tdata)
            .def_ro("s_axis_tvalid", &Rx::s_axis_tvalid)
            .def_ro("s_axis_tready", &Rx::s_axis_tready)
            // Drives tready.
            .def_prop_rw("ready", [](Rx &self)
                         { return self.ready(); }, [](Rx &self, uint8_t ready)
                         { self.ready(ready); })
            // Triggers after every accepted beat.
            .def_prop_ro("received", &Rx::received, nb::rv_policy::reference_internal)
            // Awaitable: resumes with the next n samples, or fewer once the timeout (> 0) elapses.
            .def("receive", [](Rx &self, size_t n, uint64_t timeout)
                 { return py_task(self.context(), self.receive(n, timeout)); }, nb::arg("n"), nb::arg("timeout") = 0)
            // Remove and return up to n samples now.
            .def("take", &Rx::take, nb::arg("n"))
            // Received samples still queued.
            .def_prop_ro("data", [](Rx &self)
                         { return std::vector<T>(self.fifo.begin(), self.fifo.end()); })
            .def("__len__", &Rx::size)
            .def("clear", &Rx::clear);
    }

    template <typename T>
    static inline auto bind_axis_tx(nb::module_ &m, const char *name)
    {
        using Tx = AxisTx<T>;
        return nb::class_<Tx, Module>(m, name)
            .def(nb::new_([](const std::string &name)
                          { return Model::create<Tx>(name); }),
                 nb::arg("name"))
            .def_ro("clk", &Tx::clk)
            .def_ro("rst", &Tx::rst)
            .def_ro("m_axis_tdata", &Tx::m_axis_tdata)
            .def_ro("m_axis_tvalid", &Tx::m_axis_tvalid)
            .def_ro("m_axis_tready", &Tx::m_axis_tready)
            // Triggers after every beat accepted by the sink.
            .def_prop_ro("sent", &Tx::sent, nb::rv_policy::reference_internal)
            // Awaitable: queues the data and resumes with True once it was all accepted, False on timeout (> 0).
            .def("send", [](Tx &self, std::vector<T> data, uint64_t timeout)
                 { return py_task(self.context(), self.send(std::move(data), timeout)); }, nb::arg("data"), nb::arg("timeout") = 0)
            // Awaitable: resumes with True once the queue is empty, False on timeout (> 0).
            .def("drain", [](Tx &self, uint64_t timeout)
                 { return py_task(self.context(), self.drain(timeout)); }, nb::arg("timeout") = 0)
            .def("push", &Tx::push_back, nb::arg("value"))
            .def("push_range", [](Tx &self, const std::vector<T> &data)
                 { self.push_range(data); }, nb::arg("data"))
            // Samples still queued.
            .def_prop_ro("data", [](Tx &self)
                         { return std::vector<T>(self.fifo.begin(), self.fifo.end()); })
            .def("__len__", &Tx::size)
            .def("clear", &Tx::clear);
    }

} // namespace dspsim::bindings
