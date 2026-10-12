/*
    Registration functions of the dspsim._framework module.

    The bindings are compiled as several translation units, one per group of classes, so that they build in
    parallel and a change to one binding header only rebuilds the groups that use it. Each function below
    registers one group and is defined in src/bindings/<group>.cpp. The helpers that bind a single class
    (bind_signal_class<T>, bind_input<T>, ...) are in the other headers of this directory.

    To bind a new class, add its helper to a header in this directory and call it from the group it belongs
    to. A new group needs a declaration here, a .cpp file in src/bindings/ that is listed in the top-level
    CMakeLists.txt, and a call in src/_framework.cpp.
*/
#pragma once
#include <nanobind/nanobind.h>

namespace dspsim::bindings
{
    namespace nb = nanobind;

    /// Context, Model, events, Channel, processes, awaitables and tasks, ModuleName and Module. (engine.cpp)
    void register_engine(nb::module_ &m);
    /// SignalBase, Signal*, BitSel and the bit functions, DerivedSignal*, Clock. (signals.cpp)
    void register_signals(nb::module_ &m);
    /// SignalArray* and SignalArrayView*. (signal_arrays.cpp)
    void register_signal_arrays(nb::module_ &m);
    /// PortBase, Input* and Output*. (ports.cpp)
    void register_ports(nb::module_ &m);
    /// InputArray* and InputArrayView*. (input_arrays.cpp)
    void register_input_arrays(nb::module_ &m);
    /// OutputArray* and OutputArrayView*. (output_arrays.cpp)
    void register_output_arrays(nb::module_ &m);
    /// SignalStruct, SignalStructArray, PortStruct and PortStructArray. (structs.cpp)
    void register_structs(nb::module_ &m);
    /// The modules that ship with dspsim: Dff*, AxisRx*, AxisTx*. (modules.cpp)
    void register_modules(nb::module_ &m);
} // namespace dspsim::bindings
