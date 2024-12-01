from dspsim import framework
from dspsim.framework import Context, Clock, signal, dff
from dspsim.library import SomeModel

import numpy as np
from typing import TypeAlias
from dspsim import util

# def signal_array(
#     shape: tuple, stype: type[Signal8 | Signal16 | Signal32], init: int = 0
# ):
#     """"""
#     if isinstance(shape, tuple):
#         return [signal_array(s)]
#     return stype(init)


"""
int x[2][3] =
[
    [1, 2, 3],
    [4, 5, 6]
]

"""


# @framework.runner(time_unit=1e-9, time_precision=1e-9)
def test_context_basic():

    # print()
    # x = shaped_list((6,), int)
    # print(x)
    # return
    # y = np.array(x)
    # print(y.shape)
    # return
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)
    x = dff(clk, 42, width=24)
    y = signal(width=24)

    # c = [Signal32(10 * x) for x in range(SomeModel.NC)]
    c = signal(18, initial=43, shape=(SomeModel.NC,))
    d = signal(18, shape=(SomeModel.ND, SomeModel.MD))
    e = signal(18, shape=(SomeModel.NE, SomeModel.ME))

    # print(SomeModel.IMAT)
    # return
    print(context.print_info())
    some_model = SomeModel(clk, rst, x, y, c, d, e)
    print(context.print_info())
    # return

    some_model.trace("traces/some_model.vcd")

    context.elaborate()

    context.advance(100)
    print(context.print_info())
    rst.d = 0
    context.advance(100)

    for _ in range(10):
        x.d = x.q + 1
        context.advance(10)

    # for _c in c:
    #     print(_c.q)
