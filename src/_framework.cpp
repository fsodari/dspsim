#include "dspsim/bindings.h"

namespace nb = nanobind;
using namespace dspsim;

NB_MODULE(_framework, m)
{
  m.doc() = "nanobind hello module...";
  // nb::literals
  m.def("hello_from_bin", &dspsim::hello_from_bin);
  m.def("foo42", []()
        { return 42; });

  // auto cf = nanobind::class_<ContextFactory>(m, "ContextFactory");
  // bind_module_context(m);
  bind_context(m, "Context");
  bind_base_model(m, "Model");
  bind_signal<uint8_t>(m, "Signal8");
  bind_signal<uint16_t>(m, "Signal16");
  bind_signal<uint32_t>(m, "Signal32");
  bind_signal<uint64_t>(m, "Signal64");

  bind_dff<uint8_t>(m, "Dff8");
  bind_dff<uint16_t>(m, "Dff16");
  bind_dff<uint32_t>(m, "Dff32");
  bind_dff<uint64_t>(m, "Dff64");
  bind_clock(m, "Clock");

  bind_axis_tx<uint8_t>(m, "AxisTx8");
  bind_axis_tx<uint16_t>(m, "AxisTx16");
  bind_axis_tx<uint32_t>(m, "AxisTx32");
  bind_axis_tx<uint64_t>(m, "AxisTx64");
  bind_axis_rx<uint8_t>(m, "AxisRx8");
  bind_axis_rx<uint16_t>(m, "AxisRx16");
  bind_axis_rx<uint32_t>(m, "AxisRx32");
  bind_axis_rx<uint64_t>(m, "AxisRx64");

  // nanobind::set_leak_warnings(false);
}
