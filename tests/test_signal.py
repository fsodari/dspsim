from dspsim import framework
from dspsim.framework import Context, Clock, signal, dff
from dspsim.library import SomeModel, Foo, X

import numpy as np
from typing import TypeAlias
from dspsim import util


# @framework.runner(time_unit=1e-9, time_precision=1e-9)
def test_context_basic():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)
    x = dff(clk, 42, width=24)
    y = signal(width=24)
    z = signal(width=24)

    c = signal(43, width=18, shape=(SomeModel.NC,))
    d = signal(width=18, shape=(SomeModel.ND, SomeModel.MD))
    e = signal(width=18, shape=(SomeModel.NE, SomeModel.ME))

    print(context.print_info())
    some_model = SomeModel(clk, rst, x, y, c, d, e)

    xmodel = X()
    foo = Foo(clk, rst, x, z)
    print(context.print_info())

    some_model.trace("traces/some_model.vcd")
    foo.trace("traces/foo.vcd")

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
