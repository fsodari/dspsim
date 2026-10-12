#include "bindings/framework.h"

#include "bindings/struct_bindings.h"

namespace dspsim::bindings
{
    void register_structs(nb::module_ &m)
    {
        bind_signal_struct(m, "SignalStruct");
        bind_signal_struct_array(m, "SignalStructArray");
        bind_port_struct(m, "PortStruct");
        bind_port_struct_array(m, "PortStructArray");
    }
} // namespace dspsim::bindings
