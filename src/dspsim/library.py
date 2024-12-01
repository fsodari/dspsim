# from dspsim._library import *
from dspsim._library import Foo, SomeModel, X, Gain
from dspsim._library import Skid as _Skid
import dspsim.axis as _axis
from dspsim.framework import Signal8 as _Signal8, SignalT as _SignalT


class Skid(_Skid):
    """Wrapper to allow connecting axis busses"""

    @classmethod
    def from_axis(
        cls, clk: _Signal8, rst: _Signal8, s_axis: _axis.Axis, m_axis: _axis.Axis
    ):
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
        )
