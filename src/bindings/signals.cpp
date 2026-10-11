#include "bindings/framework.h"

#include "bindings/signal_bindings.h"
#include "bindings/bitsel_bindings.h"
#include "bindings/clock_bindings.h"

namespace dspsim::bindings
{
    void register_signals(nb::module_ &m)
    {
        // Bind Signals
        bind_signal_base(m, "SignalBase");

        bind_signal_class<uint8_t>(m, "SignalU8");
        bind_signal_class<uint16_t>(m, "SignalU16");
        bind_signal_class<uint32_t>(m, "SignalU32");
        bind_signal_class<uint64_t>(m, "SignalU64");

        bind_signal_class<int8_t>(m, "SignalS8");
        bind_signal_class<int16_t>(m, "SignalS16");
        bind_signal_class<int32_t>(m, "SignalS32");
        bind_signal_class<int64_t>(m, "SignalS64");

        bind_signal_class<double>(m, "SignalFloat");

        // Bit selections
        bind_bitsel(m, "BitSel");
        bind_bit_functions(m);

        bind_derived_signal<uint8_t>(m, "DerivedSignalU8");
        bind_derived_signal<uint16_t>(m, "DerivedSignalU16");
        bind_derived_signal<uint32_t>(m, "DerivedSignalU32");
        bind_derived_signal<uint64_t>(m, "DerivedSignalU64");

        bind_derived_signal<int8_t>(m, "DerivedSignalS8");
        bind_derived_signal<int16_t>(m, "DerivedSignalS16");
        bind_derived_signal<int32_t>(m, "DerivedSignalS32");
        bind_derived_signal<int64_t>(m, "DerivedSignalS64");

        // Clock
        bind_clock(m, "Clock");
    }
} // namespace dspsim::bindings
