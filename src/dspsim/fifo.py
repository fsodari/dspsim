from dspsim.framework import Signal8
from dspsim.axis import Axis

from dspsim._library import FifoAsync as _FifoAsync, FifoSync as _FifoSync
from dspsim._library import FifoAsync8 as _FifoAsync8


class FifoAsync(_FifoAsync):
    @classmethod
    def init_bus(
        cls,
        clka: Signal8,
        rsta: Signal8,
        s_axis: Axis,
        clkb: Signal8,
        rstb: Signal8,
        m_axis: Axis,
    ):
        return cls(
            clka,
            rsta,
            s_axis.tdata,
            s_axis.tvalid,
            s_axis.tready,
            s_axis.tid,
            clkb,
            rstb,
            m_axis.tdata,
            m_axis.tvalid,
            m_axis.tready,
            m_axis.tid,
        )


class FifoAsync8(_FifoAsync8):
    @classmethod
    def init_bus(
        cls,
        clka: Signal8,
        rsta: Signal8,
        s_axis: Axis,
        clkb: Signal8,
        rstb: Signal8,
        m_axis: Axis,
    ):
        return cls(
            clka,
            rsta,
            s_axis.tdata,
            s_axis.tvalid,
            s_axis.tready,
            s_axis.tid,
            clkb,
            rstb,
            m_axis.tdata,
            m_axis.tvalid,
            m_axis.tready,
            m_axis.tid,
        )


class FifoSync(_FifoSync):
    @classmethod
    def init_bus(cls, clk: Signal8, rst: Signal8, s_axis: Axis, m_axis: Axis):
        """"""
        return cls(
            clk=clk,
            rst=rst,
            s_axis_tdata=s_axis.tdata,
            s_axis_tvalid=s_axis.tvalid,
            s_axis_tready=s_axis.tready,
            s_axis_tid=s_axis.tid,
            m_axis_tdata=m_axis.tdata,
            m_axis_tvalid=m_axis.tvalid,
            m_axis_tready=m_axis.tready,
            m_axis_tid=m_axis.tid,
        )
