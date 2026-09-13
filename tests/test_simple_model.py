from pathlib import Path

from dspsim._framework import SimpleModel

from dspsim.framework import Clock, Context, Dff8, Signal8


def test_simple_model():
    """Test the SimpleModel creation and basic evaluation steps."""
    with Context() as ctx:
        with ctx.construct():
            clk = Clock(10, name="clk")
            rst = Dff8(clk, 1, name="rst")
            i = Dff8(clk, name="i")
            o1 = Signal8(name="o1")
            o2 = Signal8(name="o2")

            a = SimpleModel(clk, rst, i, o1, name="a")
            b = SimpleModel(clk, rst, o1, o2, name="b")

        # Trace functions be called after context timescale is set.
        ctx.set_timescale("1ns", "1ns")
        # a.trace(Path("traces") / "a.trace")
        # b.trace(Path("traces") / "b.trace")

        rst.d = 1
        ctx.run(100)
        rst.d = 0
        ctx.run(10)

        for iter in range(10):
            i.d = iter
            ctx.run(10)
            print(o1.q, o2.q)

        # print(f"a id: {a.id}, b id: {b.id}")
        for m in ctx.models:
            print(m)
        # a.close()
        # b.close()
    print("Here?")
    # ctx.clear()
