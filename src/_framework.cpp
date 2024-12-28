#include "dspsim/bindings.h"

namespace nb = nanobind;
using namespace dspsim;

double sc_time_stamp(void)
{
  return 0;
}

NB_MODULE(_framework, m)
{
  m.doc() = "DSPSim Framework Module";

  // Context
  bind_context(m, "Context");

  // Model
  bind_base_model(m, "Model");

  // Signals
  bind_signal<uint8_t>(m, "Signal8");
  bind_signal<uint16_t>(m, "Signal16");
  bind_signal<uint32_t>(m, "Signal32");
  bind_signal<uint64_t>(m, "Signal64");
  // bind_signal<uint8_t>(m, "SignalU8");
  // bind_signal<uint16_t>(m, "SignalU16");
  // bind_signal<uint32_t>(m, "SignalU32");
  // bind_signal<uint64_t>(m, "SignalU64");

  // Dffs
  bind_dff<uint8_t>(m, "Dff8");
  bind_dff<uint16_t>(m, "Dff16");
  bind_dff<uint32_t>(m, "Dff32");
  bind_dff<uint64_t>(m, "Dff64");
  // bind_dff<uint8_t>(m, "DffU8");
  // bind_dff<uint16_t>(m, "DffU16");
  // bind_dff<uint32_t>(m, "DffU32");
  // bind_dff<uint64_t>(m, "DffU64");

  // Clock
  bind_clock(m, "Clock");

  // Axis Tx
  bind_axis_tx<uint8_t>(m, "AxisTx8");
  bind_axis_tx<uint16_t>(m, "AxisTx16");
  bind_axis_tx<uint32_t>(m, "AxisTx32");
  bind_axis_tx<uint64_t>(m, "AxisTx64");
  // bind_axis_tx<uint8_t>(m, "AxisTxU8");
  // bind_axis_tx<uint16_t>(m, "AxisTxU16");
  // bind_axis_tx<uint32_t>(m, "AxisTxU32");
  // bind_axis_tx<uint64_t>(m, "AxisTxU64");

  // Axis Rx
  bind_axis_rx<uint8_t>(m, "AxisRx8");
  bind_axis_rx<uint16_t>(m, "AxisRx16");
  bind_axis_rx<uint32_t>(m, "AxisRx32");
  bind_axis_rx<uint64_t>(m, "AxisRx64");
  // bind_axis_rx<uint8_t>(m, "AxisRxU8");
  // bind_axis_rx<uint16_t>(m, "AxisRxU16");
  // bind_axis_rx<uint32_t>(m, "AxisRxU32");
  // bind_axis_rx<uint64_t>(m, "AxisRxU64");

  bind_axil_m<uint32_t, uint32_t>(m, "AxilM32");

  // Wishbone Master
  bind_wisbone_m<uint32_t, uint8_t>(m, "WishboneM8");
  bind_wisbone_m<uint32_t, uint16_t>(m, "WishboneM16");
  bind_wisbone_m<uint32_t, uint32_t>(m, "WishboneM32");
  bind_wisbone_m<uint32_t, uint64_t>(m, "WishboneM64");
  // bind_wisbone_m<uint32_t, uint8_t>(m, "WishboneMU8");
  // bind_wisbone_m<uint32_t, uint16_t>(m, "WishboneMU16");
  // bind_wisbone_m<uint32_t, uint32_t>(m, "WishboneMU32");
  // bind_wisbone_m<uint32_t, uint64_t>(m, "WishboneMU64");

  _bind_context_factory(m, "ContextFactory");
  bind_global_context(m);

  // nanobind::set_leak_warnings(false);
}
