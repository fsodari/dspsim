from pathlib import Path

from dspsim._framework import SimpleModel

from dspsim.framework import Clock, Context, Signal8, Simulator


def test_simple_model():
    """Test the SimpleModel creation and basic evaluation steps."""
    with Context() as ctx:
        with ctx.construct():
            clk = Clock(10)
            rst = Signal8()
            i = Signal8()
            o1 = Signal8()
            o2 = Signal8()

            a = SimpleModel(clk, rst, i, o1)
            b = SimpleModel(clk, rst, o1, o2)

        sim = Simulator(ctx)
        # Trace functions be called after sim.
        a.trace(Path("traces") / "a.vcd")
        b.trace(Path("traces") / "b.vcd")
        for iter in range(10):
            i.d = iter
            sim.run(10)
            print(o1.q, o2.q)

        print(f"a id: {a.id}, b id: {b.id}")
        for m in ctx.models:
            print(f"Model id: {m.id}, kind: {m.kind}, parent_id: {m.parent_id}")
        # a.close()
        # b.close()
