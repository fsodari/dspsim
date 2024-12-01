from dspsim.framework import Model, Signal8, SignalT, signal
from typing import TypeVar
import numpy as _np
from numpy.typing import ArrayLike as _ArrayLike
import functools as _functools


class Axis:
    tdata: SignalT
    tvalid: Signal8
    tready: Signal8

    def __init__(self, *, width: int):
        self.tdata = signal(width=width)
        self.tvalid = signal()
        self.tready = signal()


class AxisTx(Model):
    """
    Python framework model for simple data streaming on an AXI-Stream bus.
    This should be replaced with a C++ model with proper bindings.
    """

    clk: Signal8
    rst: Signal8
    tdata: SignalT
    tvalid: Signal8
    tready: Signal8

    _buf: list[int]

    def __init__(
        self,
        clk: Signal8,
        rst: Signal8,
        tdata: SignalT,
        tvalid: Signal8,
        tready: Signal8,
    ):
        """"""
        # Initialize the Model base class.
        super().__init__()
        # Python creates shared ptrs. Register this model with the context.
        self.context.own_model(self)

        self.clk = clk
        self.rst = rst
        self.tdata = tdata
        self.tvalid = tvalid
        self.tready = tready
        self._buf = []

    @classmethod
    def init_axis(cls, clk: Signal8, rst: Signal8, axis: Axis):
        """"""
        return cls(clk, rst, axis.tdata, axis.tvalid, axis.tready)

    def eval_step(self) -> None:
        if self.clk.posedge():
            if self.tvalid.q and self.tready.q:
                self.tvalid.d = 0

            if len(self._buf):
                # Send new data if the output stream is not stalled.
                if not self.tvalid.q or self.tready.q:
                    self.tdata.d = self._buf.pop(0)
                    self.tvalid.d = 1

    # @_functools.singledispatchmethod
    # def write(self, arg):
    #     raise Exception("Not Iplemnts")

    # @write.register
    # def _(self, x: int):
    #     """"""
    #     self._buf.append(x)

    # @write.register
    # def _(self, x: list):
    #     self._buf.extend(x)

    # @write.register
    # def _(self, x: _np.ndarray):
    #     """"""
    #     self._buf.extend(x.astype(_np.int32))

    def write(self, x, float_q: int = None, sign_extend: int = None):
        """"""
        if float_q:
            qm = 2**float_q
        else:
            qm = 1
        try:
            _xiter = iter(x)
            if isinstance(x, _np.ndarray):
                tx_data = x * qm
                self._buf.extend(tx_data.astype(_np.int_))
            else:
                tx_data = [int(qm * _x) for _x in x]
                self._buf.extend(tx_data)

        except TypeError:
            self._buf.append(int(x * qm))


class AxisRx(Model):
    clk: Signal8
    rst: Signal8
    tdata: SignalT
    tvalid: Signal8
    tready: Signal8

    _buf: list[int]
    _next_ready: int

    def __init__(
        self,
        clk: Signal8,
        rst: Signal8,
        tdata: SignalT,
        tvalid: Signal8,
        tready: Signal8,
    ):
        """"""
        # Initialize the Model base class.
        super().__init__()
        # Python creates shared ptrs. Register this model with the context.
        self.context.own_model(self)

        self.clk = clk
        self.rst = rst
        self.tdata = tdata
        self.tvalid = tvalid
        self.tready = tready
        self._buf = []
        self._next_ready = 0

    @classmethod
    def init_axis(cls, clk: Signal8, rst: Signal8, axis: Axis):
        """"""
        return cls(clk, rst, axis.tdata, axis.tvalid, axis.tready)

    @property
    def ready(self):
        return self._next_ready

    @ready.setter
    def ready(self, value: int):
        self._next_ready = value

    def eval_step(self) -> None:
        if self.clk.posedge():
            self.tready.d = self._next_ready
            if self.tvalid.q and self.tready.q:
                self._buf.append(self.tdata.q)

    def read(
        self, clear: bool = True, float_q: int = None, sign_extend: int = None
    ) -> _ArrayLike:
        """"""
        res = _np.array(self._buf.copy())
        if clear:
            self._buf.clear()

        if float_q:
            qm = 1.0 / (2**float_q)
            return res.astype(_np.double) * qm
        return res


M = TypeVar("M", bound=Model)


def init_stream_model[
    M
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
