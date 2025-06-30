from dspsim._util import ErrorCode, DType, AvrilCommand, AvrilMode
import struct


def dtype_lookup_name(s: str) -> DType:
    """Get dtype from name."""
    return DType._member_map_[s]


def dtype_lookup_code(code: int) -> DType:
    return DType._value2member_map_[code]


_fmt_lookup_tbl = {
    DType.x: "x",
    DType.int8: "b",
    DType.uint8: "B",
    DType.int16: "h",
    DType.uint16: "H",
    DType.int32: "l",
    DType.uint32: "L",
    DType.int64: "q",
    DType.uint64: "Q",
    DType.float: "f",
    DType.double: "d",
    # DType.str4: "4s",
    # DType.str8: "8s",
    # DType.str16: "16s",
    # DType.str32: "32s",
    # DType.str64: "64s",
}


def unpack_dtype(b: bytes, dtype: DType):
    """Unpack bytes into data"""
    if dtype.is_str:
        fmt = f"<{len(b)}s"
    else:
        n = int(len(b) / dtype.size)
        fmt = f"<{n}{_fmt_lookup_tbl[dtype]}"
    return struct.unpack(fmt, b)


def pack_dtype(*data, dtype: DType) -> bytes:
    fmt = "<"
    for d in data:
        if dtype.is_str:
            # fmt = f"<{len(d)}s"
            fmt += f"{len(d)}s"
        else:
            fmt += _fmt_lookup_tbl[dtype]
    return struct.pack(fmt, *data)


def cnv_dtype(data, dtype: DType):
    if dtype.is_anyint:
        return int(data)
    if dtype.is_float:
        return float(data)
    if dtype.is_str:
        return str(data)


__all__ = ["ErrorCode", "DType", "AvrilCommand", "AvrilMode"]
