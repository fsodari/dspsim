#include "bindings/framework.h"

#include "bindings/port_bindings.h"

namespace dspsim::bindings
{
    void register_output_arrays(nb::module_ &m)
    {
        bind_output_array<uint8_t>(m, "OutputArrayU8");
        bind_output_array<uint16_t>(m, "OutputArrayU16");
        bind_output_array<uint32_t>(m, "OutputArrayU32");
        bind_output_array<uint64_t>(m, "OutputArrayU64");

        bind_output_array<int8_t>(m, "OutputArrayS8");
        bind_output_array<int16_t>(m, "OutputArrayS16");
        bind_output_array<int32_t>(m, "OutputArrayS32");
        bind_output_array<int64_t>(m, "OutputArrayS64");

        bind_output_array<double>(m, "OutputArrayFloat");

        bind_output_array_view<uint8_t>(m, "OutputArrayViewU8");
        bind_output_array_view<uint16_t>(m, "OutputArrayViewU16");
        bind_output_array_view<uint32_t>(m, "OutputArrayViewU32");
        bind_output_array_view<uint64_t>(m, "OutputArrayViewU64");

        bind_output_array_view<int8_t>(m, "OutputArrayViewS8");
        bind_output_array_view<int16_t>(m, "OutputArrayViewS16");
        bind_output_array_view<int32_t>(m, "OutputArrayViewS32");
        bind_output_array_view<int64_t>(m, "OutputArrayViewS64");

        bind_output_array_view<double>(m, "OutputArrayViewFloat");
    }
} // namespace dspsim::bindings
