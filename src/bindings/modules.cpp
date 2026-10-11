#include "bindings/framework.h"

#include "bindings/dff_bindings.h"
#include "bindings/axis_bindings.h"

namespace dspsim::bindings
{
    void register_modules(nb::module_ &m)
    {
        // Dff
        bind_dff_class<uint8_t>(m, "DffU8");
        bind_dff_class<uint16_t>(m, "DffU16");
        bind_dff_class<uint32_t>(m, "DffU32");
        bind_dff_class<uint64_t>(m, "DffU64");

        bind_dff_class<int8_t>(m, "DffS8");
        bind_dff_class<int16_t>(m, "DffS16");
        bind_dff_class<int32_t>(m, "DffS32");
        bind_dff_class<int64_t>(m, "DffS64");

        // AXI-Stream source/sink
        bind_axis_rx<uint8_t>(m, "AxisRxU8");
        bind_axis_rx<uint16_t>(m, "AxisRxU16");
        bind_axis_rx<uint32_t>(m, "AxisRxU32");
        bind_axis_rx<uint64_t>(m, "AxisRxU64");
        bind_axis_rx<int8_t>(m, "AxisRxS8");
        bind_axis_rx<int16_t>(m, "AxisRxS16");
        bind_axis_rx<int32_t>(m, "AxisRxS32");
        bind_axis_rx<int64_t>(m, "AxisRxS64");

        bind_axis_tx<uint8_t>(m, "AxisTxU8");
        bind_axis_tx<uint16_t>(m, "AxisTxU16");
        bind_axis_tx<uint32_t>(m, "AxisTxU32");
        bind_axis_tx<uint64_t>(m, "AxisTxU64");
        bind_axis_tx<int8_t>(m, "AxisTxS8");
        bind_axis_tx<int16_t>(m, "AxisTxS16");
        bind_axis_tx<int32_t>(m, "AxisTxS32");
        bind_axis_tx<int64_t>(m, "AxisTxS64");
    }
} // namespace dspsim::bindings
