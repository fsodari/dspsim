from dspsim import framework
from dspsim.framework import Context, Clock, signal, dff, SignalT, Signal8, Model
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.library import Skid


class BaseClass:
    clk: Signal8
    rst: Signal8
    s_axis_tdata: SignalT
    s_axis_tvalid: Signal8
    s_axis_tready: Signal8
    m_axis_tdata: SignalT
    m_axis_tvalid: Signal8
    m_axis_tready: Signal8


from typing import TypeVar

M = TypeVar("M", bound=Model)


def init_stream_model[
    M: Model
](
    cls: type[M],
    clk: Signal8,
    rst: Signal8,
    s_axis: Axis,
    m_axis: Axis,
    *extra,
    **extrak,
) -> M:
    """"""
    return cls(
        clk,
        rst,
        s_axis.tdata,
        s_axis.tvalid,
        s_axis.tready,
        m_axis.tdata,
        m_axis.tvalid,
        m_axis.tready,
        *extra,
        **extrak,
    )


# @framework.runner(time_unit=1e-9, time_precision=1e-9)
def test_skid_basic():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    b0 = Axis(width=Skid.DW)
    b1 = Axis(width=Skid.DW)
    b2 = Axis(width=Skid.DW)
    print(context.print_info())

    # skid0 = Skid(
    #     clk, rst, b0.tdata, b0.tvalid, b0.tready, b1.tdata, b1.tvalid, b1.tready
    # )
    # skid0 = Skid.from_axis(clk, rst, b0, b1)
    skid0 = init_stream_model(Skid, clk, rst, b0, b1)
    skid1 = Skid.from_axis(clk, rst, b1, b2)

    print(context.print_info())
    axis_tx = AxisTx.init_axis(clk, rst, b0)
    axis_rx = AxisRx.init_axis(clk, rst, b2)

    print(context.print_info())

    skid0.trace("traces/skid0.vcd")
    skid1.trace("traces/skid1.vcd")

    context.elaborate()

    context.advance(100)

    rst.d = 0
    context.advance(100)

    axis_tx.write([1, 2, 3, 4, 5])
    context.advance(100)
    # b1.tready.d = 1
    axis_rx.ready = 1
    context.advance(100)

    rx_data = axis_rx.read()
    print(rx_data)
    # for i in range(10):
    #     b0.tdata.d = i
    #     b0.tvalid.d = 1
    #     context.advance(10)
    #     b0.tvalid.d = 0
    #     context.advance(10)

    # for _c in c:
    #     print(_c.q)
