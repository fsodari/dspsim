"""
The SomeModel.sv component contains every type of parameter and port.
It can be used to test code generation and bus connection.
"""

from dspsim.framework import Context, Clock, signal, dff
from dspsim.library import SomeModel

from pathlib import Path

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_context_basic():
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = dff(clk, 1)
        x = dff(clk, 42, width=24)
        y = signal(width=24)

        c = signal(43, width=18, shape=(SomeModel.NC,))
        d = signal(width=18, shape=(SomeModel.ND, SomeModel.MD))
        e = signal(width=18, shape=(SomeModel.NE, SomeModel.ME))

        some_model = SomeModel(clk, rst, x, y, c, d, e)
        some_model.trace(trace_dir / "some_model.vcd")

        context.elaborate()
        print(context)

        rst.d = 1
        context.run(100)
        rst.d = 0
        context.run(100)

        for _ in range(10):
            x.d = x.q + 1
            context.run(10)
