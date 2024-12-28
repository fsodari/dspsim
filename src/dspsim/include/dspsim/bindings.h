#pragma once
#include "dspsim/dspsim.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/filesystem.h>
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
              .def("run", &Context::run, nb::arg("time_inc") = 1)
              //     .def("own_model", &Context::own_model, nb::arg("model"))
              .def_prop_ro("models", &Context::models, nb::rv_policy::reference)
              .def("__repr__", &Context::_repr)
              .def("__str__", &Context::_str);
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
              .def(nb::new_(&Signal<T>::create),
                   nb::arg("initial") = 0,
                   nb::arg("width") = default_bitwidth<T>::value,
                   nb::arg("signed") = false)
              //     .def(nb::new_([](int initial)
              //                   { return Signal<T>::create(initial); }),
              //          nb::arg("initial") = 0)
              .def_prop_rw("width", &Signal<T>::get_width, &Signal<T>::set_width)
              .def_prop_rw("sign_extend", &Signal<T>::get_sign_extend, &Signal<T>::set_sign_extend, nb::arg("extend"))
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
              .def(nb::new_(&Dff<T>::create),
                   nb::arg("clk"),
                   nb::arg("initial") = 0,
                   nb::arg("width") = default_bitwidth<T>::value,
                   nb::arg("signed") = false);
     }

     // Bind Clock.
     static inline auto bind_clock(nb::handle &scope, const char *name)
     {
          return nb::class_<Clock, Signal<uint8_t>>(scope, name)
              .def(nb::new_(&Clock::create),
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
                   nb::arg("m_axis_tlast") = nb::none(),
                   nb::arg("tid_pattern") = std::list<uint8_t>{0})
              .def("write_command", nb::overload_cast<T>(&AxisTx<T>::write_command),
                   nb::arg("data"))
              .def("write_command", nb::overload_cast<std::vector<T> &>(&AxisTx<T>::write_command),
                   nb::arg("data"))
              .def("write_command", nb::overload_cast<double, int>(&AxisTx<T>::writef_command),
                   nb::arg("data"), nb::arg("q") = 0)
              .def("write_command", nb::overload_cast<std::vector<double> &, int>(&AxisTx<T>::writef_command),
                   nb::arg("data"), nb::arg("q") = 0)
              .def("write", nb::overload_cast<T, int>(&AxisTx<T>::write_block),
                   nb::arg("data"), nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<std::vector<T> &, int>(&AxisTx<T>::write_block),
                   nb::arg("data"), nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<double, int, int>(&AxisTx<T>::writef_block),
                   nb::arg("data"), nb::arg("q") = 0, nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<std::vector<double> &, int, int>(&AxisTx<T>::writef_block),
                   nb::arg("data"), nb::arg("q") = 0, nb::arg("timeout") = -1);
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
                   nb::arg("s_axis_tid") = nb::none(),
                   nb::arg("s_axis_tlast") = nb::none())
              .def_prop_rw("width", &AxisRx<T>::get_width, &AxisRx<T>::set_width, nb::arg("width") = default_bitwidth<T>::value)
              .def_prop_rw("tready", &AxisRx<T>::get_tready, &AxisRx<T>::set_tready, nb::arg("value"))
              .def("read_rx_buf", &AxisRx<T>::read_rx_buf, nb::arg("clear") = -1)
              .def("read_tid", &AxisRx<T>::read_tid, nb::arg("clear") = -1)
              .def("read", nb::overload_cast<int>(&AxisRx<T>::read_block),
                   nb::arg("timeout") = -1)
              .def("read", nb::overload_cast<int, int>(&AxisRx<T>::read_block),
                   nb::arg("n"), nb::arg("timeout") = -1)
              .def("read", nb::overload_cast<int, int, int>(&AxisRx<T>::readf_block),
                   nb::arg("n"), nb::arg("q"), nb::arg("timeout") = -1);
     }

     template <typename AT, typename DT>
     static inline auto bind_wisbone_m(nb::handle &scope, const char *name)
     {
          using WBM = WishboneM<AT, DT>;
          return nb::class_<WBM>(scope, name)
              .def(nb::new_(&WBM::create),
                   nb::arg("clk"),
                   nb::arg("rst"),
                   nb::arg("cyc_o"),
                   nb::arg("stb_o"),
                   nb::arg("we_o"),
                   nb::arg("ack_i"),
                   nb::arg("stall_i"),
                   nb::arg("addr_o"),
                   nb::arg("data_o"),
                   nb::arg("data_i"))
              .def("command", &WBM::command, nb::arg("mode"), nb::arg("address"), nb::arg("data") = 0)
              .def("clear", &WBM::clear)
              .def_prop_ro("busy", &WBM::busy)
              // Read commands.
              .def("read_command", nb::overload_cast<AT, size_t>(&WBM::read_command),
                   nb::arg("start_address"), nb::arg("n") = 1)
              .def("read_command", nb::overload_cast<std::vector<AT> &>(&WBM::read_command),
                   nb::arg("addresses"))

              // Rx data
              .def_prop_ro("rx_size", &WBM::rx_size)
              .def("rx_data", &WBM::rx_data, nb::arg("amount") = -1)
              .def("rx_data", &WBM::rx_dataf, nb::arg("q"), nb::arg("amount") = -1)

              // Blocking reads.
              .def("read", nb::overload_cast<AT, int>(&WBM::read_block),
                   nb::arg("address"), nb::arg("timeout") = -1)
              .def("read", nb::overload_cast<std::vector<AT> &, int>(&WBM::read_block),
                   nb::arg("addresses"), nb::arg("timeout") = -1)
              .def("read", nb::overload_cast<AT, int, int>(&WBM::readf_block),
                   nb::arg("address"), nb::arg("q"), nb::arg("timeout") = -1)
              .def("read", nb::overload_cast<std::vector<AT> &, int, int>(&WBM::readf_block),
                   nb::arg("addresses"), nb::arg("q"), nb::arg("timeout") = -1)

              // Write commands.
              .def("write_command", nb::overload_cast<AT, DT>(&WBM::write_command),
                   nb::arg("address"), nb::arg("data"))
              .def("write_command", nb::overload_cast<AT, std::vector<DT> &>(&WBM::write_command),
                   nb::arg("start_address"), nb::arg("data"))
              .def("write_command", nb::overload_cast<std::map<AT, DT> &>(&WBM::write_command),
                   nb::arg("data"))

              .def("write_command", nb::overload_cast<AT, double, int>(&WBM::writef_command),
                   nb::arg("address"), nb::arg("data"), nb::arg("q"))
              .def("write_command", nb::overload_cast<AT, std::vector<double> &, int>(&WBM::writef_command),
                   nb::arg("start_address"), nb::arg("data"), nb::arg("q"))
              .def("write_command", nb::overload_cast<std::map<AT, double> &, int>(&WBM::writef_command),
                   nb::arg("data"), nb::arg("q"))

              // Blocking writes.
              .def("write", nb::overload_cast<AT, DT, int>(&WBM::write_block),
                   nb::arg("address"), nb::arg("data"), nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<AT, std::vector<DT> &, int>(&WBM::write_block),
                   nb::arg("start_address"), nb::arg("data"), nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<std::map<AT, DT> &, int>(&WBM::write_block),
                   nb::arg("data"), nb::arg("timeout") = -1)

              .def("write", nb::overload_cast<AT, double, int, int>(&WBM::writef_block),
                   nb::arg("address"), nb::arg("data"), nb::arg("q"), nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<AT, std::vector<double> &, int, int>(&WBM::writef_block),
                   nb::arg("start_address"), nb::arg("data"), nb::arg("q"), nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<std::map<AT, double> &, int, int>(&WBM::writef_block),
                   nb::arg("data"), nb::arg("q"), nb::arg("timeout") = -1)
              //     .def("write", [](WBM &wbm, std::map<int, int> d, int timeout)
              //          { wbm.write_block(d, timeout); }, nb::arg("data"), nb::arg("timeout") = 10000)
              // getitem, setitem accessors.
              .def("__getitem__", [](WBM &wbm, AT address)
                   { return wbm.read_block(address, -1); }, nb::arg("address"))
              .def("__setitem__", [](WBM &wbm, AT address, DT data)
                   { wbm.write_block(address, data, -1); }, nb::arg("address"), nb::arg("data"));
     }

     template <typename AT, typename DT>
     static inline auto bind_axil_m(nb::handle &scope, const char *name)
     {
          using AM = AxilM<AT, DT>;
          return nb::class_<AM>(scope, name)
              .def(nb::new_(&AM::create),
                   nb::arg("clk"),
                   nb::arg("rst"),
                   nb::arg("m_axil_awaddr"),
                   nb::arg("m_axil_awvalid"),
                   nb::arg("m_axil_awready"),
                   nb::arg("m_axil_wdata"),
                   nb::arg("m_axil_wvalid"),
                   nb::arg("m_axil_wready"),
                   nb::arg("m_axil_bresp"),
                   nb::arg("m_axil_bvalid"),
                   nb::arg("m_axil_bready"),
                   nb::arg("m_axil_araddr"),
                   nb::arg("m_axil_arvalid"),
                   nb::arg("m_axil_arready"),
                   nb::arg("m_axil_rdata"),
                   nb::arg("m_axil_rresp"),
                   nb::arg("m_axil_rvalid"),
                   nb::arg("m_axil_rready"))
              .def_prop_rw("bready", &AM::get_bready, &AM::set_bready)
              .def_prop_rw("rready", &AM::get_rready, &AM::set_rready)
              // Non blocking commands and buffer reads.
              .def("write_command", nb::overload_cast<AT, DT>(&AM::write_command), nb::arg("address"), nb::arg("data"))
              .def("write_command", nb::overload_cast<AT, std::vector<DT> &>(&AM::write_command), nb::arg("start_address"), nb::arg("data"))
              .def("write_command", nb::overload_cast<std::map<AT, DT> &>(&AM::write_command), nb::arg("data"))
              .def("read_bresp_buf", &AM::read_bresp_buf, nb::arg("amount") = -1)
              .def("read_command", nb::overload_cast<AT>(&AM::read_command), nb::arg("address"))
              .def("read_command", nb::overload_cast<std::vector<AT> &>(&AM::read_command), nb::arg("addresses"))
              .def("read_rdata_buf", &AM::read_rdata_buf, nb::arg("amount") = -1)
              .def("read_rresp_buf", &AM::read_rresp_buf, nb::arg("amount") = -1)
              // Blocking commands.
              .def("write", nb::overload_cast<AT, DT, int>(&AM::write),
                   nb::arg("address"), nb::arg("data"), nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<AT, std::vector<DT> &, int>(&AM::write),
                   nb::arg("start_address"), nb::arg("data"), nb::arg("timeout") = -1)
              .def("write", nb::overload_cast<std::map<AT, DT> &, int>(&AM::write),
                   nb::arg("data"), nb::arg("timeout") = -1)
              .def("read", nb::overload_cast<AT, int>(&AM::read),
                   nb::arg("address"), nb::arg("timeout") = -1)
              .def("read", nb::overload_cast<std::vector<AT> &, int>(&AM::read),
                   nb::arg("addresses"), nb::arg("timeout") = -1)
              .def("__getitem__", [](AM &axil, AT address)
                   { return std::get<0>(axil.read(address, -1)); })
              .def("__setitem__", [](AM &axil, AT address, DT data)
                   { axil.write(address, data, -1); });
     }
} // namespace dspsim
