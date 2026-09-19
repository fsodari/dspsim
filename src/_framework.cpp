#include "bindings.h"

using namespace dspsim;

NB_MODULE(_framework, m)
{
    m.doc() = "dspsim framework module";

    // Bind the Context
    bind_context(m, "Context");
    // Bind ContextFactory and global context factory functions
    bind_context_factory(m, "ContextFactory");
    // Bind Model base class
    bind_model(m, "Model");
    // Bind TimeEvent
    bind_time_event(m, "TimeEvent");
    // // Bind SensitivityEvent
    // bind_sensitivity_event(m, "SensitivityEvent");

    // Bind Signals
    bind_signal_class<uint8_t>(m, "Signal8");
    bind_signal_class<uint16_t>(m, "Signal16");
    bind_signal_class<uint32_t>(m, "Signal32");
    bind_signal_class<uint64_t>(m, "Signal64");

    // Bind Ports
    bind_input<uint8_t>(m, "Input8");
    bind_input<uint16_t>(m, "Input16");
    bind_input<uint32_t>(m, "Input32");
    bind_input<uint64_t>(m, "Input64");
    bind_output<uint8_t>(m, "Output8");
    bind_output<uint16_t>(m, "Output16");
    bind_output<uint32_t>(m, "Output32");
    bind_output<uint64_t>(m, "Output64");

    // Sensitivity List
    bind_sensitivity_list(m, "SensitivityList");

    // Module
    bind_module_name(m, "ModuleName");
    bind_module(m, "Module");

    // Clock
    bind_clock(m, "Clock");

    // Dff
    bind_dff_class<uint8_t>(m, "Dff8");
    bind_dff_class<uint16_t>(m, "Dff16");
    bind_dff_class<uint32_t>(m, "Dff32");
    bind_dff_class<uint64_t>(m, "Dff64");
}

double sc_time_stamp(void)
{
    return 0.0;
}
