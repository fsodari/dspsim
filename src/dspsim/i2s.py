from dspsim.framework import Signal8, signal
from dspsim.axis import Axis

from dspsim._library import I2SClkGen as _I2SClkGen
from dspsim._library import I2STx as _I2STx
from dspsim._library import I2SRx as _I2SRx


class I2S:
    mclk: Signal8
    lrclk: Signal8
    sclk: Signal8
    sd: Signal8

    def __init__(
        self, mclk: Signal8, lrclk: Signal8 = None, sclk: Signal8 = None
    ) -> None:
        # Clocks will be asosciated with a bus.
        self.mclk = mclk
        self.lrclk = lrclk if lrclk else signal()
        self.sclk = sclk if sclk else signal()
        # Create a data signal for this bus.
        self.sd = signal()


class I2SClkGen(_I2SClkGen):
    @classmethod
    def init_bus(cls, clk: Signal8, rst: Signal8, i2s: I2S):
        return cls(clk, rst, i2s.lrclk, i2s.sclk)


class I2STx(_I2STx):
    @classmethod
    def init_bus(cls, clk: Signal8, rst: Signal8, s_axis: Axis, i2s: I2S):
        """"""
        return cls(
            clk,
            rst,
            s_axis.tdata,
            s_axis.tvalid,
            s_axis.tready,
            s_axis.tid,
            i2s.lrclk,
            i2s.sclk,
            i2s.sd,
        )


class I2SRx(_I2SRx):
    @classmethod
    def init_bus(cls, clk: Signal8, rst: Signal8, m_axis: Axis, i2s: I2S):
        """"""
        return cls(
            clk,
            rst,
            m_axis.tdata,
            m_axis.tvalid,
            m_axis.tready,
            m_axis.tid,
            i2s.lrclk,
            i2s.sclk,
            i2s.sd,
        )
