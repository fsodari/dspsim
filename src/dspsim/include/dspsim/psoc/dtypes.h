#pragma once
#include <stdint.h>

#define DTYPE_UINT_ID (0 << 16)
#define DTYPE_INT_ID (1 << 16)
#define DTYPE_FLT_ID (2 << 16)
#define DTYPE_CMPLX_ID (3 << 16)
#define DTYPE_STR_ID (4 << 16)

typedef enum DType
{
    // Undefined.
    dX = 0,
    // stdint types.
    dint8 = DTYPE_INT_ID | 1,
    duint8 = DTYPE_UINT_ID | 1,
    dint16 = DTYPE_INT_ID | 2,
    duint16 = DTYPE_UINT_ID | 2,
    dint32 = DTYPE_INT_ID | 4,
    duint32 = DTYPE_UINT_ID | 4,
    dint64 = DTYPE_INT_ID | 8,
    duint64 = DTYPE_UINT_ID | 8,
    dint128 = DTYPE_INT_ID | 16, // Is this a thing?
    duint128 = DTYPE_UINT_ID | 16,

    // float and double.
    dfloat = DTYPE_FLT_ID | 4,
    ddouble = DTYPE_FLT_ID | 8,

    // Complex types. complex8 complex16, complex32, complex64, complex128
    dcomplex8 = DTYPE_CMPLX_ID | 2,
    dcomplex16 = DTYPE_CMPLX_ID | 4,
    dcomplex32 = DTYPE_CMPLX_ID | 8,
    dcomplex64 = DTYPE_CMPLX_ID | 16,
    dcomplex128 = DTYPE_CMPLX_ID | 32,

    // Strings are on the top bit (and only top bit) set;
    dstr4 = DTYPE_STR_ID | 4,   // 4 byte string.
    dstr8 = DTYPE_STR_ID | 8,   // 8 byte string
    dstr16 = DTYPE_STR_ID | 16, // 16 byte string
    dstr32 = DTYPE_STR_ID | 32, // 32 byte string
    dstr64 = DTYPE_STR_ID | 64,
} DType;

uint32_t dtype_size(DType dtype);

static inline int dtype_check(DType dtype, uint32_t id) { return (dtype >> 16) == (id >> 16); }
static inline int dtype_is_str(DType dtype) { return dtype_check(dtype, DTYPE_STR_ID); }
