from dspsim.framework import Context, Clock, signal, dff, SignalT, Signal8, Model
from dspsim.axis import Axis, AxisTx, AxisRx, init_stream_model
from dspsim.library import Gain, FifoSync
import numpy as np
from dspsim.util import to_fixed, to_float


def test_gain_basic():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    b0 = Axis(width=Gain.DW, tid=True)
    b1 = Axis(width=Gain.DW, tid=True)
    b2 = Axis(width=Gain.DW)

    _k = to_fixed(0.1, Gain.COEFQ)
    k = signal(int(_k), width=Gain.COEFW)

    print(context.print_info())

    fifo = FifoSync.init_bus(clk, rst, b0, b1)
    gain = Gain.init_bus(clk, rst, b1, b2, gain=k)
    axis_tx = AxisTx.init_bus(clk, rst, b0)
    axis_rx = AxisRx.init_bus(clk, rst, b2)

    print(context.print_info())

    gain.trace("traces/gain.vcd")

    context.elaborate()

    context.advance(100)

    rst.d = 0
    context.advance(100)

    DATAQ = 16
    tx_data = np.linspace(1, 10.0, 10)
    print(np.round(tx_data, 3))
    axis_tx.write(tx_data, DATAQ)
    context.advance(100)
    # b1.tready.d = 1
    axis_rx.ready = 1
    context.advance(200)

    rx_data = axis_rx.read(float_q=DATAQ)
    print(np.round(rx_data, 3))