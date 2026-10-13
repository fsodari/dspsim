#include "bindings/framework.h"

#include "bindings/port_bindings.h"

namespace dspsim::bindings
{
    void register_ports(nb::module_ &m)
    {
        // Bind Ports
        bind_port_base(m, "PortBase");

        bind_input<uint8_t>(m, "InputU8");
        bind_input<uint16_t>(m, "InputU16");
        bind_input<uint32_t>(m, "InputU32");
        bind_input<uint64_t>(m, "InputU64");

        bind_input<int8_t>(m, "InputS8");
        bind_input<int16_t>(m, "InputS16");
        bind_input<int32_t>(m, "InputS32");
        bind_input<int64_t>(m, "InputS64");

        bind_input<double>(m, "InputFloat");

        bind_output<uint8_t>(m, "OutputU8");
        bind_output<uint16_t>(m, "OutputU16");
        bind_output<uint32_t>(m, "OutputU32");
        bind_output<uint64_t>(m, "OutputU64");

        bind_output<int8_t>(m, "OutputS8");
        bind_output<int16_t>(m, "OutputS16");
        bind_output<int32_t>(m, "OutputS32");
        bind_output<int64_t>(m, "OutputS64");

        bind_output<double>(m, "OutputFloat");

        // Arrays of ports without the type of their value
        bind_port_base_array(m, "PortBaseArrayView");
    }
} // namespace dspsim::bindings
