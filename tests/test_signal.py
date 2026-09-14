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
        with ctx.construct():
            s8 = signal(init=1, width=8)
            assert isinstance(s8, Signal8)

            s16 = signal(init=1, width=16)
            assert isinstance(s16, Signal16)

            s32 = signal(init=1, width=32, is_signed=True)
            assert isinstance(s32, Signal32)

            s64 = signal(init=1, width=64)
            assert isinstance(s64, Signal64)

            try:
                signal(init=1, width=128)
            except ValueError as e:
                assert str(e) == "Unsupported signal width"

        print(ctx.models)
