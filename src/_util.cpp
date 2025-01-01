/*
    Bindings for misc utilities.
    Includes conversion functions for audio data and simulation data.
    Includes bindings of psoc Config definitions. Error codes, dtypes, etc. Single definition principle.
*/

// #include "dspsim/bindings.h"
#include "nanobind/nanobind.h"

#include "dspsim/psoc/error_codes.h"
#include "dspsim/psoc/mmi_dtypes.h"
#include "dspsim/psoc/avril_defs.h"

namespace nb = nanobind;

uint32_t _dtype_size(MMIDtypes dtype)
{
    return std::abs(dtype) & 0xFFFF;
}

NB_MODULE(_util, m)
{
    nb::enum_<dErrorCodes>(m, "ErrorCode")
        .value("NoError", dErrorCodes::dErrNone)
        .value("Overflow", dErrorCodes::dErrOverflow)
        .value("InvalidAddress", dErrorCodes::dErrInvalidAddress)
        .value("ReadOnly", dErrorCodes::dErrReadOnly)
        .value("WriteOnly", dErrorCodes::dErrWriteOnly)
        .value("AddrAlign16", dErrorCodes::dErrAddrAlign16)
        .value("AddrAlign32", dErrorCodes::dErrAddrAlign32)
        .value("AddrAlign64", dErrorCodes::dErrAddrAlign64)
        .value("AddrAlignN", dErrorCodes::dErrAddrAlignN)
        .value("SizeAlign16", dErrorCodes::dErrSizeAlign16)
        .value("SizeAlign32", dErrorCodes::dErrSizeAlign32)
        .value("SizeAlign64", dErrorCodes::dErrSizeAlign64)
        .value("SizeAlignN", dErrorCodes::dErrSizeAlignN)
        .export_values();

    nb::enum_<MMIDtypes>(m, "DType")
        .def_prop_ro("size", [](MMIDtypes &dtype)
                     { return _dtype_size(dtype); })
        .value("x", MMIDtypes::MMI_x)
        .value("b", MMIDtypes::MMI_b)
        .value("B", MMIDtypes::MMI_B)
        .value("h", MMIDtypes::MMI_h)
        .value("H", MMIDtypes::MMI_H)
        .value("l", MMIDtypes::MMI_l)
        .value("L", MMIDtypes::MMI_L)
        .value("q", MMIDtypes::MMI_q)
        .value("Q", MMIDtypes::MMI_Q)
        .value("f", MMIDtypes::MMI_f)
        .value("d", MMIDtypes::MMI_d)
        .export_values();

    nb::enum_<AvrilCommand>(m, "AvrilCommand")
        .value("Nop", AvrilCommand::AvrilNop)
        .value("Write", AvrilCommand::AvrilWrite)
        .value("Read", AvrilCommand::AvrilRead)
        .value("NopAck", AvrilCommand::AvrilNopAck)
        .value("WriteAck", AvrilCommand::AvrilWriteAck)
        .value("ReadAck", AvrilCommand::AvrilReadAck)
        .export_values();

    nb::enum_<AvrilMode>(m, "AvrilMode")
        .value("Vmmi", AvrilMode::AvrilVmmi)
        .value("Bootload", AvrilMode::AvrilBootload)
        .value("VMeta", AvrilMode::AvrilVMeta)
        .export_values();

    m.def("dtype_size", &_dtype_size, nb::arg("dtype"));
}