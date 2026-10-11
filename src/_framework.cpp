#include "bindings/framework.h"

using namespace dspsim;

NB_MODULE(_framework, m)
{
    m.doc() = "dspsim framework module";

    // Each group is its own translation unit: see bindings/framework.h.
    // The order matters: a base class must be registered before the classes derived from it
    // (Model before signals, ports and Module; Signal* before DerivedSignal*, Clock and the arrays).
    bindings::register_engine(m);
    bindings::register_signals(m);
    bindings::register_signal_arrays(m);
    bindings::register_ports(m);
    bindings::register_input_arrays(m);
    bindings::register_output_arrays(m);
    bindings::register_modules(m);
}

double sc_time_stamp(void)
{
    return 0.0;
}
