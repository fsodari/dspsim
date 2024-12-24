from dspsim.framework import Context, Clock, dff
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.library import Skid
import numpy as np
from pathlib import Path

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_skid_basic():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    b0 = Axis(width=Skid.DW)
    b1 = Axis(width=Skid.DW)
    b2 = Axis(width=Skid.DW)

    skid0 = Skid(clk, rst, *b0, *b1)
    skid1 = Skid(clk, rst, *b1, *b2)

    axis_tx = AxisTx(clk, rst, b0)
    axis_rx = AxisRx(clk, rst, b2)

    skid0.trace(trace_dir / "skid0.vcd")
    skid1.trace(trace_dir / "skid1.vcd")

    context.elaborate()
    print(context)

    context.run(100)

    rst.d = 0
    context.run(100)

    tx_data = list(range(1, 6))
    axis_tx.write_command(tx_data)
    context.run(100)
    axis_rx.tready = 1
    context.run(100)

    rx_data = axis_rx.read_rx_buf()

    assert np.all(tx_data == rx_data)
