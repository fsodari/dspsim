# from dspsim._framework import *

# from dspsim._framework import get_context_factory
from dspsim._framework import Context, global_context
from dspsim._framework import Model
from dspsim._framework import Clock, Signal8, Signal16, Signal32, Signal64
# from dspsim._framework import SignalU8, SignalU16, SignalU32, SignalU64

from dspsim._framework import Dff8, Dff16, Dff32, Dff64
# from dspsim._framework import DffU8, DffU16, DffU32, DffU64

import contextlib as _contextlib
import functools as _functools
from dspsim import util as _util
from typing import TypeVar

from dspsim.config import Port as _Port

SignalT = (
    Signal8 | Signal16 | Signal32 | Signal64
    # | SignalU8
    # | SignalU16
    # | SignalU32
    # | SignalU64
)
DffT = Dff8 | Dff16 | Dff32 | Dff64  # | DffU8 | DffU16 | DffU32 | DffU64

ModelT = TypeVar("ModelT", bound=Model)


def _sclass(width: int) -> type[SignalT]:
    """"""
    uw = _util.uint_width(width)
    _types = {8: Signal8, 16: Signal16, 32: Signal32, 64: Signal64}
    return _types[uw]
    # _utypes = {8: SignalU8, 16: SignalU16, 32: SignalU32, 64: SignalU64}
    # return _types[uw] if signed else _utypes[uw]


def _dffclass(width: int) -> type[DffT]:
    """"""
    uw = _util.uint_width(width)
    _types = {8: Dff8, 16: Dff16, 32: Dff32, 64: Dff64}
    return _types[uw]
    # _utypes = {8: DffU8, 16: DffU16, 32: DffU32, 64: DffU64}
    # return _types[uw] if signed else _utypes[uw]


def _signal(initial: int = 0, *, width: int = 1, signed: bool = False) -> SignalT:
    """Create a signal of the correct stdint type based on the bitwidth."""
    return _sclass(width)(initial, width, signed)


def signal(
    initial: int = 0, *, width: int = 1, signed: bool = False, shape: tuple = ()
) -> SignalT | list[SignalT]:
    """
    Create a signal or signal array with the appropriate shape.
    This builds up the list recursively based on the shape.
    """
    if len(shape):
        return [
            signal(initial, width=width, signed=signed, shape=shape[1:])
            for i in range(shape[0])
        ]
    return _signal(initial, width=width, signed=signed)


def _dff(
    clk: Signal8, initial: int = 0, *, width: int = 1, signed: bool = False
) -> DffT:
    """Create a signal of the correct stdint type based on the bitwidth."""
    return _dffclass(width)(clk, initial, width, signed)


def dff(
    clk: Signal8,
    initial: int = 0,
    *,
    width: int = 1,
    signed: bool = False,
    shape: tuple = (),
) -> DffT | list[DffT]:
    """"""
    if len(shape):
        return [
            dff(clk, initial, width=width, signed=signed, shape=shape[1:])
            for _ in range(shape[0])
        ]
    return _dff(clk, initial, width=width, signed=signed)


@_contextlib.contextmanager
def enter_context(time_unit: float = 1e-9, time_precision: float = 1e-9):
    context = Context(time_unit, time_precision)
    try:
        yield context
    finally:
        context.clear()


def runner(time_unit: float = 1e-9, time_precision: float = 1e-9):
    """"""

    def runner_deco(func):
        @_functools.wraps(func)
        def wrapped():
            context = Context(time_unit, time_precision)
            result = func(context)
            context.clear()
            return result

        return wrapped

    return runner_deco


def port_info(model: Model) -> dict[str, _Port]:
    """"""
    import ast

    linfo = ast.literal_eval(model.port_info)
    return {k: _Port(**v) for k, v in linfo.items()}
