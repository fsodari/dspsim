from dspsim.framework import Context, Clock, signal, dff
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.library import Gain, FifoSync
from dspsim.util import to_fixed, to_float

import numpy as np
from pathlib import Path

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_gain_basic():
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = dff(clk, 1)

        b0 = Axis(width=Gain.DW, tid=True)
        b1 = Axis(width=Gain.DW, tid=True)
        b2 = Axis(width=Gain.DW)

        _k = to_fixed(0.1, Gain.COEFQ)
        k = signal(int(_k), width=Gain.COEFW)

        print(context)

        fifo = FifoSync.init_bus(clk, rst, b0, b1)
        gain = Gain.init_bus(clk, rst, b1, b2, gain=k)
        axis_tx = AxisTx(clk, rst, b0)
        axis_rx = AxisRx(clk, rst, b2)

        print(context)

        fifo.trace(trace_dir / "gain_fifo.vcd")
        gain.trace(trace_dir / "gain.vcd")

        context.elaborate()

        context.run(100)

        rst.d = 0
        context.run(100)

        DATAQ = 16
        tx_data = np.linspace(1, 10.0, 10)
        axis_tx.write_command(tx_data, DATAQ)
        context.run(100)
        axis_rx.tready = True
        context.run(200)

        rx_data = to_float(np.array(axis_rx.read_rx_buf()), DATAQ)
        assert np.all(np.isclose(rx_data * 10, tx_data, rtol=0.0001))
