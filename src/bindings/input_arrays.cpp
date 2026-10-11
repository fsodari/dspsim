#include "bindings/framework.h"

#include "bindings/port_bindings.h"

namespace dspsim::bindings
{
    void register_input_arrays(nb::module_ &m)
    {
        bind_input_array<uint8_t>(m, "InputArrayU8");
        bind_input_array<uint16_t>(m, "InputArrayU16");
        bind_input_array<uint32_t>(m, "InputArrayU32");
        bind_input_array<uint64_t>(m, "InputArrayU64");

        bind_input_array<int8_t>(m, "InputArrayS8");
        bind_input_array<int16_t>(m, "InputArrayS16");
        bind_input_array<int32_t>(m, "InputArrayS32");
        bind_input_array<int64_t>(m, "InputArrayS64");

        bind_input_array<double>(m, "InputArrayFloat");

        bind_input_array_view<uint8_t>(m, "InputArrayViewU8");
        bind_input_array_view<uint16_t>(m, "InputArrayViewU16");
        bind_input_array_view<uint32_t>(m, "InputArrayViewU32");
        bind_input_array_view<uint64_t>(m, "InputArrayViewU64");

        bind_input_array_view<int8_t>(m, "InputArrayViewS8");
        bind_input_array_view<int16_t>(m, "InputArrayViewS16");
        bind_input_array_view<int32_t>(m, "InputArrayViewS32");
        bind_input_array_view<int64_t>(m, "InputArrayViewS64");

        bind_input_array_view<double>(m, "InputArrayViewFloat");
    }
} // namespace dspsim::bindings
