from dspsim.framework import Context, Clock, signal, dff, SignalT, Signal8, Model
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.library import Skid


def test_skid_basic():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    b0 = Axis(width=Skid.DW)
    b1 = Axis(width=Skid.DW)
    b2 = Axis(width=Skid.DW)
    print(context.print_info())

    skid0 = Skid.init_bus(clk, rst, b0, b1)
    skid1 = Skid.init_bus(clk, rst, b1, b2)

    print(context.print_info())
    axis_tx = AxisTx.init_bus(clk, rst, b0)
    axis_rx = AxisRx.init_bus(clk, rst, b2)

    print(context.print_info())

    skid0.trace("traces/skid0.vcd")
    skid1.trace("traces/skid1.vcd")

    context.elaborate()

    context.advance(100)

    rst.d = 0
    context.advance(100)

    axis_tx.write([1, 2, 3, 4, 5])
    context.advance(100)
    axis_rx.ready = 1
    context.advance(100)

    rx_data = axis_rx.read()
    print(rx_data)