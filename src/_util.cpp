/*
    Bindings for misc utilities.
    Includes conversion functions for audio data and simulation data.
    Includes bindings of psoc Config definitions. Error codes, dtypes, etc. Single definition principle.
*/

// #include "dspsim/bindings.h"
#include "nanobind/nanobind.h"

extern "C"
{
#include "dspsim/psoc/error_codes.h"
#include "dspsim/psoc/dtypes.h"
#include "dspsim/psoc/avril.h"
}

namespace nb = nanobind;

uint32_t _dtype_size(DType &dtype)
{
    return std::abs(dtype) & 0xFFFF;
}

int _dtype_is_int(DType &dtype)
{
    return dtype_check(dtype, DTYPE_INT_ID);
}

int _dtype_is_uint(DType &dtype)
{
    return dtype_check(dtype, DTYPE_UINT_ID);
}

int _dtype_is_anyint(DType &dtype)
{
    return _dtype_is_uint(dtype) || _dtype_is_int(dtype);
}

int _dtype_is_float(DType &dtype)
{
    return dtype_check(dtype, DTYPE_FLT_ID);
}

int _dtype_is_complex(DType &dtype)
{
    return dtype_check(dtype, DTYPE_CMPLX_ID);
}

int _dtype_is_str(DType &dtype)
{
    return dtype_is_str(dtype);
}

NB_MODULE(_util, m)
{
    nb::enum_<dErrorCodes>(m, "ErrorCode")
        .value("NoError", dErrorCodes::dErrNone)
        .value("Overflow", dErrorCodes::dErrOverflow)
        .value("InvalidMode", dErrorCodes::dErrInvalidMode)
        .value("InvalidCommand", dErrorCodes::dErrInvalidCommand)
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
        .value("KeyNotFound", dErrorCodes::dErrKeyNotFound)
        .export_values();

    nb::enum_<DType>(m, "DType")
        .def_prop_ro("size", &_dtype_size)
        .def_prop_ro("is_uint", &_dtype_is_uint)
        .def_prop_ro("is_int", &_dtype_is_int)
        .def_prop_ro("is_anyint", &_dtype_is_anyint)
        .def_prop_ro("is_float", &_dtype_is_float)
        .def_prop_ro("is_complex", &_dtype_is_complex)
        .def_prop_ro("is_str", &_dtype_is_str)
        .value("x", DType::dX)
        .value("int8", DType::dint8)
        .value("uint8", DType::duint8)
        .value("int16", DType::dint16)
        .value("uint16", DType::duint16)
        .value("int32", DType::dint32)
        .value("uint32", DType::duint32)
        .value("int64", DType::dint64)
        .value("uint64", DType::duint64)
        .value("int128", DType::dint128)
        .value("uint128", DType::duint128)
        .value("float", DType::dfloat)
        .value("double", DType::ddouble)
        .value("complex8", DType::dcomplex8)
        .value("complex16", DType::dcomplex16)
        .value("complex32", DType::dcomplex32)
        .value("complex64", DType::dcomplex64)
        .value("complex128", DType::dcomplex128)
        .value("str4", DType::dstr4)
        .value("str8", DType::dstr8)
        .value("str16", DType::dstr16)
        .value("str32", DType::dstr32)
        .value("str64", DType::dstr64)

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
}