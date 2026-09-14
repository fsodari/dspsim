from dspsim.framework import Context, Clock, Dff8, Dff32, Signal8, Signal32
from pathlib import Path
from some_example.library import SomeModel


def test_some_model():
    assert SomeModel.DW == 24

    with Context() as ctx:
        with ctx.construct():
            clk = Clock(10, name="clk")
            rst = Dff8(clk, 1, name="rst")
            i = Signal32(SomeModel.DW)
            o = Signal32(SomeModel.DW)
            some_model = SomeModel(clk, rst, i, o, name="some_model")

        # ctx.set_timescale("1ns", "1ns")
        some_model.trace(Path("traces/some_model.vcd"))

        rst.d = 1
        ctx.run(100)
        rst.d = 0
        ctx.run(10)
        for x in range(10):
            i.d = x
            ctx.run(10)
            print(o.q)
