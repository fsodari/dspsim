from dspsim._framework import *
import contextlib as _contextlib
import functools as _functools
from dspsim import util as _util

SignalT = Signal8 | Signal16 | Signal32
DffT = Dff8 | Dff16 | Dff32


def _sclass(width: int) -> type[SignalT]:
    """"""
    _types = {8: Signal8, 16: Signal16, 32: Signal32}
    return _types[_util.uint_width(width)]


def _dffclass(width: int) -> type[DffT]:
    """"""
    _types = {8: Dff8, 16: Dff16, 32: Dff32}
    return _types[_util.uint_width(width)]


def _signal(width: int, initial: int = 0) -> SignalT:
    """Create a signal of the correct stdint type based on the bitwidth."""
    return _sclass(width)(initial)


def signal(
    width: int = 1, initial: int = 0, *, shape: tuple = ()
) -> SignalT | list[SignalT]:
    """
    Create a signal or signal array with the appropriate shape.
    This builds up the list recursively based on the shape.
    """
    if len(shape):
        return [signal(width, initial, shape=shape[1:]) for i in range(shape[0])]
    return _signal(width, initial)


def _dff(clk: Signal8, initial: int = 0, *, width: int = 1) -> DffT:
    """Create a signal of the correct stdint type based on the bitwidth."""
    return _dffclass(width)(clk, initial)


def dff(
    clk: Signal8, initial: int = 0, *, width: int = 1, shape: tuple = ()
) -> DffT | list[DffT]:
    """"""
    if len(shape):
        return [
            dff(clk, initial, width=width, shape=shape[1:]) for _ in range(shape[0])
        ]
    return _dff(clk, initial, width=width)


@_contextlib.contextmanager
def enter_context(time_unit: float = 1e-9, time_precision: float = 1e-9):
    context = Context(time_unit, time_precision)
    try:
        yield context
    finally:
        context.exit_context()


def runner(time_unit: float = 1e-9, time_precision: float = 1e-9):
    """"""

    def runner_deco(func):
        @_functools.wraps(func)
        def wrapped():
            context = Context(time_unit, time_precision)
            result = func(context)
            context.exit_context()
            return result

        return wrapped

    return runner_deco
