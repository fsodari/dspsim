# from dspsim._framework import *

# from dspsim._framework import get_context_factory
from dspsim._framework import Context, global_context
from dspsim._framework import Model, Clock
from dspsim._framework import Signal8, Signal16, Signal32, Signal64
from dspsim._framework import Dff8, Dff16, Dff32, Dff64

import contextlib as _contextlib
import functools as _functools
from dspsim import util as _util
from typing import TypeVar, Literal

SignalT = Signal8 | Signal16 | Signal32 | Signal64
DffT = Dff8 | Dff16 | Dff32 | Dff64

ModelT = TypeVar("ModelT", bound=Model)


def _sclass(width: int) -> type[SignalT]:
    """"""
    _types = {8: Signal8, 16: Signal16, 32: Signal32, 64: Signal64}
    return _types[_util.uint_width(width)]


def _dffclass(width: int) -> type[DffT]:
    """"""
    _types = {8: Dff8, 16: Dff16, 32: Dff32, 64: Dff64}
    return _types[_util.uint_width(width)]


def _signal(initial: int = 0, *, width: int = 1) -> SignalT:
    """Create a signal of the correct stdint type based on the bitwidth."""
    return _sclass(width)(initial)


def signal(
    initial: int = 0, *, width: int = 1, shape: tuple = ()
) -> SignalT | list[SignalT]:
    """
    Create a signal or signal array with the appropriate shape.
    This builds up the list recursively based on the shape.
    """
    if len(shape):
        return [signal(initial, width=width, shape=shape[1:]) for i in range(shape[0])]
    return _signal(initial, width=width)


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


from dspsim.config import Port as _Port


def port_info(model: Model) -> dict[str, _Port]:
    """"""
    import ast

    linfo = ast.literal_eval(model.port_info)
    return {k: _Port(**v) for k, v in linfo.items()}
