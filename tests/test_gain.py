from dspsim import framework
from dspsim.framework import Context, Clock, signal, dff, SignalT, Signal8, Model
from dspsim.axis import Axis, AxisTx, AxisRx, init_stream_model
from dspsim.library import Gain
import numpy as np


def to_fixed(flt: float, q: int) -> int:
    """"""
    return flt * (2**q)


def to_float(fxd: int, q: int) -> float:
    """"""
    return fxd / (2**q)


# @framework.runner(time_unit=1e-9, time_precision=1e-9)
def test_gain_basic():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    b0 = Axis(width=Gain.DW)
    b1 = Axis(width=Gain.DW)

    _k = to_fixed(0.1, Gain.COEFQ)
    k = signal(int(_k), width=Gain.COEFW)

    print(context.print_info())

    # skid0 = Skid(
    #     clk, rst, b0.tdata, b0.tvalid, b0.tready, b1.tdata, b1.tvalid, b1.tready
    # )
    # skid0 = Skid.from_axis(clk, rst, b0, b1)
    gain = init_stream_model(Gain, clk, rst, b0, b1, k)

    print(context.print_info())
    axis_tx = AxisTx.init_axis(clk, rst, b0)
    axis_rx = AxisRx.init_axis(clk, rst, b1)

    print(context.print_info())

    gain.trace("traces/gain.vcd")

    context.elaborate()

    context.advance(100)

    rst.d = 0
    context.advance(100)

    # tx_data =
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
