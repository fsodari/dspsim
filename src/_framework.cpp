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

  bind_module_context(m);
  bind_context(m, "Context");
  bind_base_model(m, "Model");
  bind_signal<uint8_t>(m, "Signal8");
  bind_signal_array<uint8_t>(m, "Signal8");
  bind_signal<uint16_t>(m, "Signal16");
  bind_signal_array<uint16_t>(m, "Signal16");
  bind_signal<uint32_t>(m, "Signal32");
  bind_signal_array<uint32_t>(m, "Signal32");
  bind_signal<uint64_t>(m, "Signal64");
  bind_signal_array<uint64_t>(m, "Signal64");

  bind_dff<uint8_t>(m, "Dff8");
  bind_dff<uint16_t>(m, "Dff16");
  bind_dff<uint32_t>(m, "Dff32");
  bind_dff<uint64_t>(m, "Dff64");
  bind_clock(m, "Clock");
}
