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
    bindings::bind_wait_result(m, "WaitResult");
    bindings::bind_wait_base(m, "WaitBase");
    bindings::bind_wait(m, "Wait");
    bindings::bind_wait_time_event(m, "WaitTimeEvent");
    bindings::bind_wait_sensitivity_event(m, "WaitSensitivityEvent");

    // C++ tasks awaitable from Python, one per result type.
    bindings::bind_task<void>(m, "Task", "None");
    bindings::bind_task<bool>(m, "TaskBool", "bool");
    bindings::bind_task<std::vector<uint8_t>>(m, "TaskListU8", "list[int]");
    bindings::bind_task<std::vector<uint16_t>>(m, "TaskListU16", "list[int]");
    bindings::bind_task<std::vector<uint32_t>>(m, "TaskListU32", "list[int]");
    bindings::bind_task<std::vector<uint64_t>>(m, "TaskListU64", "list[int]");
    bindings::bind_task<std::vector<int8_t>>(m, "TaskListS8", "list[int]");
    bindings::bind_task<std::vector<int16_t>>(m, "TaskListS16", "list[int]");
    bindings::bind_task<std::vector<int32_t>>(m, "TaskListS32", "list[int]");
    bindings::bind_task<std::vector<int64_t>>(m, "TaskListS64", "list[int]");

    // Bind Signals
    bindings::bind_signal_base(m, "SignalBase");

    bindings::bind_signal_class<uint8_t>(m, "SignalU8");
    bindings::bind_signal_class<uint16_t>(m, "SignalU16");
    bindings::bind_signal_class<uint32_t>(m, "SignalU32");
    bindings::bind_signal_class<uint64_t>(m, "SignalU64");

    bindings::bind_signal_class<int8_t>(m, "SignalS8");
    bindings::bind_signal_class<int16_t>(m, "SignalS16");
    bindings::bind_signal_class<int32_t>(m, "SignalS32");
    bindings::bind_signal_class<int64_t>(m, "SignalS64");

    bindings::bind_signal_class<double>(m, "SignalFloat");

    bindings::bind_signal_array<uint8_t>(m, "SignalArrayU8");
    bindings::bind_signal_array<uint16_t>(m, "SignalArrayU16");
    bindings::bind_signal_array<uint32_t>(m, "SignalArrayU32");
    bindings::bind_signal_array<uint64_t>(m, "SignalArrayU64");

    bindings::bind_signal_array<int8_t>(m, "SignalArrayS8");
    bindings::bind_signal_array<int16_t>(m, "SignalArrayS16");
    bindings::bind_signal_array<int32_t>(m, "SignalArrayS32");
    bindings::bind_signal_array<int64_t>(m, "SignalArrayS64");

    bindings::bind_signal_array<double>(m, "SignalArrayFloat");

    bindings::bind_signal_array_view<uint8_t>(m, "SignalArrayViewU8");
    bindings::bind_signal_array_view<uint16_t>(m, "SignalArrayViewU16");
    bindings::bind_signal_array_view<uint32_t>(m, "SignalArrayViewU32");
    bindings::bind_signal_array_view<uint64_t>(m, "SignalArrayViewU64");

    bindings::bind_signal_array_view<int8_t>(m, "SignalArrayViewS8");
    bindings::bind_signal_array_view<int16_t>(m, "SignalArrayViewS16");
    bindings::bind_signal_array_view<int32_t>(m, "SignalArrayViewS32");
    bindings::bind_signal_array_view<int64_t>(m, "SignalArrayViewS64");

    bindings::bind_signal_array_view<double>(m, "SignalArrayViewFloat");

    // Bit selections
    bindings::bind_bitsel(m, "BitSel");
    bindings::bind_bit_functions(m);

    bindings::bind_derived_signal<uint8_t>(m, "DerivedSignalU8");
    bindings::bind_derived_signal<uint16_t>(m, "DerivedSignalU16");
    bindings::bind_derived_signal<uint32_t>(m, "DerivedSignalU32");
    bindings::bind_derived_signal<uint64_t>(m, "DerivedSignalU64");

    bindings::bind_derived_signal<int8_t>(m, "DerivedSignalS8");
    bindings::bind_derived_signal<int16_t>(m, "DerivedSignalS16");
    bindings::bind_derived_signal<int32_t>(m, "DerivedSignalS32");
    bindings::bind_derived_signal<int64_t>(m, "DerivedSignalS64");

    // Clock
    bindings::bind_clock(m, "Clock");

    // Bind Ports
    bindings::bind_port_base(m, "PortBase");

    bindings::bind_input<uint8_t>(m, "InputU8");
    bindings::bind_input<uint16_t>(m, "InputU16");
    bindings::bind_input<uint32_t>(m, "InputU32");
    bindings::bind_input<uint64_t>(m, "InputU64");

    bindings::bind_input<int8_t>(m, "InputS8");
    bindings::bind_input<int16_t>(m, "InputS16");
    bindings::bind_input<int32_t>(m, "InputS32");
    bindings::bind_input<int64_t>(m, "InputS64");

    bindings::bind_input<double>(m, "InputFloat");

    bindings::bind_input_array<uint8_t>(m, "InputArrayU8");
    bindings::bind_input_array<uint16_t>(m, "InputArrayU16");
    bindings::bind_input_array<uint32_t>(m, "InputArrayU32");
    bindings::bind_input_array<uint64_t>(m, "InputArrayU64");

    bindings::bind_input_array<int8_t>(m, "InputArrayS8");
    bindings::bind_input_array<int16_t>(m, "InputArrayS16");
    bindings::bind_input_array<int32_t>(m, "InputArrayS32");
    bindings::bind_input_array<int64_t>(m, "InputArrayS64");

    bindings::bind_input_array<double>(m, "InputArrayFloat");

    bindings::bind_input_array_view<uint8_t>(m, "InputArrayViewU8");
    bindings::bind_input_array_view<uint16_t>(m, "InputArrayViewU16");
    bindings::bind_input_array_view<uint32_t>(m, "InputArrayViewU32");
    bindings::bind_input_array_view<uint64_t>(m, "InputArrayViewU64");

    bindings::bind_input_array_view<int8_t>(m, "InputArrayViewS8");
    bindings::bind_input_array_view<int16_t>(m, "InputArrayViewS16");
    bindings::bind_input_array_view<int32_t>(m, "InputArrayViewS32");
    bindings::bind_input_array_view<int64_t>(m, "InputArrayViewS64");

    bindings::bind_input_array_view<double>(m, "InputArrayViewFloat");

    bindings::bind_output<uint8_t>(m, "OutputU8");
    bindings::bind_output<uint16_t>(m, "OutputU16");
    bindings::bind_output<uint32_t>(m, "OutputU32");
    bindings::bind_output<uint64_t>(m, "OutputU64");

    bindings::bind_output<int8_t>(m, "OutputS8");
    bindings::bind_output<int16_t>(m, "OutputS16");
    bindings::bind_output<int32_t>(m, "OutputS32");
    bindings::bind_output<int64_t>(m, "OutputS64");

    bindings::bind_output<double>(m, "OutputFloat");

    bindings::bind_output_array<uint8_t>(m, "OutputArrayU8");
    bindings::bind_output_array<uint16_t>(m, "OutputArrayU16");
    bindings::bind_output_array<uint32_t>(m, "OutputArrayU32");
    bindings::bind_output_array<uint64_t>(m, "OutputArrayU64");

    bindings::bind_output_array<int8_t>(m, "OutputArrayS8");
    bindings::bind_output_array<int16_t>(m, "OutputArrayS16");
    bindings::bind_output_array<int32_t>(m, "OutputArrayS32");
    bindings::bind_output_array<int64_t>(m, "OutputArrayS64");

    bindings::bind_output_array<double>(m, "OutputArrayFloat");

    bindings::bind_output_array_view<uint8_t>(m, "OutputArrayViewU8");
    bindings::bind_output_array_view<uint16_t>(m, "OutputArrayViewU16");
    bindings::bind_output_array_view<uint32_t>(m, "OutputArrayViewU32");
    bindings::bind_output_array_view<uint64_t>(m, "OutputArrayViewU64");

    bindings::bind_output_array_view<int8_t>(m, "OutputArrayViewS8");
    bindings::bind_output_array_view<int16_t>(m, "OutputArrayViewS16");
    bindings::bind_output_array_view<int32_t>(m, "OutputArrayViewS32");
    bindings::bind_output_array_view<int64_t>(m, "OutputArrayViewS64");

    bindings::bind_output_array_view<double>(m, "OutputArrayViewFloat");

    // Module
    bindings::bind_module_name(m, "ModuleName");
    bindings::bind_module(m, "Module");

    // Dff
    bindings::bind_dff_class<uint8_t>(m, "DffU8");
    bindings::bind_dff_class<uint16_t>(m, "DffU16");
    bindings::bind_dff_class<uint32_t>(m, "DffU32");
    bindings::bind_dff_class<uint64_t>(m, "DffU64");

    bindings::bind_dff_class<int8_t>(m, "DffS8");
    bindings::bind_dff_class<int16_t>(m, "DffS16");
    bindings::bind_dff_class<int32_t>(m, "DffS32");
    bindings::bind_dff_class<int64_t>(m, "DffS64");

    // AXI-Stream source/sink
    bindings::bind_axis_rx<uint8_t>(m, "AxisRxU8");
    bindings::bind_axis_rx<uint16_t>(m, "AxisRxU16");
    bindings::bind_axis_rx<uint32_t>(m, "AxisRxU32");
    bindings::bind_axis_rx<uint64_t>(m, "AxisRxU64");
    bindings::bind_axis_rx<int8_t>(m, "AxisRxS8");
    bindings::bind_axis_rx<int16_t>(m, "AxisRxS16");
    bindings::bind_axis_rx<int32_t>(m, "AxisRxS32");
    bindings::bind_axis_rx<int64_t>(m, "AxisRxS64");

    bindings::bind_axis_tx<uint8_t>(m, "AxisTxU8");
    bindings::bind_axis_tx<uint16_t>(m, "AxisTxU16");
    bindings::bind_axis_tx<uint32_t>(m, "AxisTxU32");
    bindings::bind_axis_tx<uint64_t>(m, "AxisTxU64");
    bindings::bind_axis_tx<int8_t>(m, "AxisTxS8");
    bindings::bind_axis_tx<int16_t>(m, "AxisTxS16");
    bindings::bind_axis_tx<int32_t>(m, "AxisTxS32");
    bindings::bind_axis_tx<int64_t>(m, "AxisTxS64");
}

double sc_time_stamp(void)
{
    return 0.0;
}
