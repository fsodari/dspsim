#include "bindings/framework.h"

#include "bindings/signal_bindings.h"

namespace dspsim::bindings
{
    void register_signal_arrays(nb::module_ &m)
    {
        bind_signal_array<uint8_t>(m, "SignalArrayU8");
        bind_signal_array<uint16_t>(m, "SignalArrayU16");
        bind_signal_array<uint32_t>(m, "SignalArrayU32");
        bind_signal_array<uint64_t>(m, "SignalArrayU64");

        bind_signal_array<int8_t>(m, "SignalArrayS8");
        bind_signal_array<int16_t>(m, "SignalArrayS16");
        bind_signal_array<int32_t>(m, "SignalArrayS32");
        bind_signal_array<int64_t>(m, "SignalArrayS64");

        bind_signal_array<double>(m, "SignalArrayFloat");

        bind_signal_array_view<uint8_t>(m, "SignalArrayViewU8");
        bind_signal_array_view<uint16_t>(m, "SignalArrayViewU16");
        bind_signal_array_view<uint32_t>(m, "SignalArrayViewU32");
        bind_signal_array_view<uint64_t>(m, "SignalArrayViewU64");

        bind_signal_array_view<int8_t>(m, "SignalArrayViewS8");
        bind_signal_array_view<int16_t>(m, "SignalArrayViewS16");
        bind_signal_array_view<int32_t>(m, "SignalArrayViewS32");
        bind_signal_array_view<int64_t>(m, "SignalArrayViewS64");

        bind_signal_array_view<double>(m, "SignalArrayViewFloat");
    }
} // namespace dspsim::bindings
