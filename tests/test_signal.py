from ctypes import c_int32, c_uint32

import dspsim
from dspsim.framework import (
    Context,
    Model,
    Signal8,
    Signal16,
    Signal32,
    Signal64,
    signal,
)


def test_signal_creation():
    with Context() as ctx:
        s8 = signal("s8", init=1, width=8)
        assert s8.context.id == ctx.id
        assert isinstance(s8, Signal8)

        s16 = signal("s16", init=1, width=16)
        assert s16.context.id == ctx.id
        assert isinstance(s16, Signal16)

        s32 = signal("s32", init=1, width=32, is_signed=True)
        assert s32.context.id == ctx.id
        assert isinstance(s32, Signal32)

        s64 = signal("s64", init=1, width=64)
        assert s64.context.id == ctx.id
        assert isinstance(s64, Signal64)

        try:
            signal("s128", init=1, width=128)
        except ValueError as e:
            assert str(e) == "Unsupported signal width"

        # print(ctx.models)
