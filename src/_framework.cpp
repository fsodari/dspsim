#include "bindings/bindings.h"

using namespace dspsim;

NB_MODULE(_framework, m)
{
    m.doc() = "dspsim framework module";

    // Bind the Context
    bindings::bind_context(m, "Context");

    // Bind ContextFactory and global context factory functions
    bindings::bind_context_factory(m, "ContextFactory");

    // Bind Model base class
    bindings::bind_model(m, "Model");

    // Bind TimeEvent
    bindings::bind_time_event(m, "TimeEvent");
    // Bind SensitivityEvent
    bindings::bind_sensitivity_event(m, "SensitivityEvent");

    // Bind Process
    bindings::bind_process_base(m, "ProcessBase");
    bindings::bind_process(m, "Process");

    // Python version of coroutines.
    bindings::bind_py_task(m, "PyTask");

    // Awaitable bindings
    bindings::bind_wait_base(m, "WaitBase");
    bindings::bind_wait(m, "Wait");
    bindings::bind_wait_time_event(m, "WaitTimeEvent");
    bindings::bind_wait_sensitivity_event(m, "WaitSensitivityEvent");

    // Bind Signals
    bindings::bind_signal_base(m, "SignalBase");
    bindings::bind_signal_class<uint8_t>(m, "Signal8");
    bindings::bind_signal_class<uint16_t>(m, "Signal16");
    bindings::bind_signal_class<uint32_t>(m, "Signal32");
    bindings::bind_signal_class<uint64_t>(m, "Signal64");
    bindings::bind_signal_class<double>(m, "SignalFloat");

    // Clock
    bindings::bind_clock(m, "Clock");

    // Bind Ports
    bindings::bind_port_base(m, "PortBase");

    bindings::bind_input<uint8_t>(m, "Input8");
    bindings::bind_input<uint16_t>(m, "Input16");
    bindings::bind_input<uint32_t>(m, "Input32");
    bindings::bind_input<uint64_t>(m, "Input64");
    bindings::bind_input<double>(m, "InputFloat");

    bindings::bind_output<uint8_t>(m, "Output8");
    bindings::bind_output<uint16_t>(m, "Output16");
    bindings::bind_output<uint32_t>(m, "Output32");
    bindings::bind_output<uint64_t>(m, "Output64");
    bindings::bind_output<double>(m, "OutputFloat");

    // Module
    bindings::bind_module_name(m, "ModuleName");
    bindings::bind_module(m, "Module");

    // Bind VPorts
    bindings::bind_vinput<uint8_t>(m, "VInput8");
    bindings::bind_vinput<uint16_t>(m, "VInput16");
    bindings::bind_vinput<uint32_t>(m, "VInput32");
    bindings::bind_vinput<uint64_t>(m, "VInput64");
    bindings::bind_vinput<double>(m, "VInputFloat");
    bindings::bind_voutput<uint8_t>(m, "VOutput8");
    bindings::bind_voutput<uint16_t>(m, "VOutput16");
    bindings::bind_voutput<uint32_t>(m, "VOutput32");
    bindings::bind_voutput<uint64_t>(m, "VOutput64");
    bindings::bind_voutput<double>(m, "VOutputFloat");

    // Dff
    bindings::bind_dff_class<uint8_t>(m, "Dff8");
    bindings::bind_dff_class<uint16_t>(m, "Dff16");
    bindings::bind_dff_class<uint32_t>(m, "Dff32");
    bindings::bind_dff_class<uint64_t>(m, "Dff64");
}

double sc_time_stamp(void)
{
    return 0.0;
}
