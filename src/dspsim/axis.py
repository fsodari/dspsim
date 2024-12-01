from dspsim.framework import Model, Signal8, SignalT, signal, port_info
import numpy as _np
from numpy.typing import ArrayLike as _ArrayLike

TIDW = 8


class Axis:
    tdata: SignalT
    tvalid: Signal8
    tready: Signal8
    tid: Signal8 = None
    tlast: Signal8 = None

    _width: int

    def __init__(self, *, width: int, tid: bool = False, tlast: bool = False):
        self._width = width
        self.tdata = signal(width=width)
        self.tvalid = signal()
        self.tready = signal()
        if tid:
            self.tid = signal(width=TIDW)
        if tlast:
            self.tlast = signal()

    def __str__(self) -> str:
        return f"Axis(width={self.width}, tid={self.tid}, tlast={self.tlast})"

    @property
    def width(self):
        return self._width


import itertools


def init_stream_model[
    ModelT
](
    cls: type[ModelT],
    clk: Signal8,
    rst: Signal8,
    s_axis: Axis,
    m_axis: Axis,
    **extra,
) -> ModelT:
    """
    Init a model that contains a stream input and output using
    """
    from dspsim.config import Port

    args = dict(
        clk=clk,
        rst=rst,
        s_axis_tdata=s_axis.tdata,
        s_axis_tvalid=s_axis.tvalid,
        s_axis_tready=s_axis.tready,
        m_axis_tdata=m_axis.tdata,
        m_axis_tvalid=m_axis.tvalid,
        m_axis_tready=m_axis.tready,
    )

    portinfo = port_info(cls)

    # Connect an empty signal if the bus doesn't have it.
    # If the model doesn't have it, don't connect it.
    if "s_axis_tid" in portinfo:
        args["s_axis_tid"] = s_axis.tid if s_axis.tid else signal(width=TIDW)
    if "s_axis_tlast" in portinfo:
        args["s_axis_tlast"] = s_axis.tlast if s_axis.tlast else signal(width=1)

    if "m_axis_tid" in portinfo:
        args["m_axis_tid"] = m_axis.tid if m_axis.tid else signal(width=TIDW)
    if "m_axis_tlast" in portinfo:
        args["m_axis_tlast"] = m_axis.tlast if m_axis.tlast else signal(width=1)

    return cls(
        **args,
        **extra,
    )


class AxisTx(Model):
    """
    Python framework model for simple data streaming on an AXI-Stream bus.
    This should be replaced with a C++ model with proper bindings.
    """

    clk: Signal8
    rst: Signal8
    m_axis_tdata: SignalT
    m_axis_tvalid: Signal8
    m_axis_tready: Signal8
    m_axis_tid: Signal8 = None

    _buf: list[int]
    _tid_pattern: list[int] = [0]
    _id_iter: itertools.cycle

    def __init__(
        self,
        clk: Signal8,
        rst: Signal8,
        m_axis_tdata: SignalT,
        m_axis_tvalid: Signal8,
        m_axis_tready: Signal8,
        m_axis_tid: Signal8 = None,
        tid_pattern: list[int] = [0],
    ):
        """"""
        # Initialize the Model base class.
        super().__init__()
        # Python creates shared ptrs. Register this model with the context.
        self.context.own_model(self)

        self.clk = clk
        self.rst = rst
        self.m_axis_tdata = m_axis_tdata
        self.m_axis_tvalid = m_axis_tvalid
        self.m_axis_tready = m_axis_tready
        self.m_axis_tid = m_axis_tid
        self._tid_pattern = tid_pattern
        self._id_iter = itertools.cycle(tid_pattern)
        self._buf = []

    @classmethod
    def init_bus(
        cls, clk: Signal8, rst: Signal8, m_axis: Axis, tid_pattern: list[int] = [0]
    ):
        """"""
        return cls(
            clk=clk,
            rst=rst,
            m_axis_tdata=m_axis.tdata,
            m_axis_tvalid=m_axis.tvalid,
            m_axis_tready=m_axis.tready,
            m_axis_tid=m_axis.tid,
            tid_pattern=tid_pattern,
        )

    # @property
    # def tid_pattern(self) -> list[int]:
    #     return self._tid_pattern

    # @tid_pattern.setter
    # def tid_pattern(self, pattern: list[int]):
    #     self._tid_pattern = pattern

    def eval_step(self) -> None:
        if self.clk.posedge():
            if self.m_axis_tvalid.q and self.m_axis_tready.q:
                self.m_axis_tvalid.d = 0

            if self.rst.q:
                self.m_axis_tvalid.d = 0
            elif len(self._buf):
                # Send new data if the output stream is not stalled.
                if not self.m_axis_tvalid.q or self.m_axis_tready.q:
                    self.m_axis_tdata.d = self._buf.pop(0)
                    if self.m_axis_tid:
                        self.m_axis_tid.d = next(self._id_iter)
                    self.m_axis_tvalid.d = 1

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
    s_axis_tdata: SignalT
    s_axis_tvalid: Signal8
    s_axis_tready: Signal8
    tid: Signal8 = None

    _buf: list[int]
    _tid_buf: list[int]
    _next_ready: int

    def __init__(
        self,
        clk: Signal8,
        rst: Signal8,
        s_axis_tdata: SignalT,
        s_axis_tvalid: Signal8,
        s_axis_tready: Signal8,
        s_axis_tid: Signal8 = None,
    ):
        """"""
        # Initialize the Model base class.
        super().__init__()
        # Python creates shared ptrs. Register this model with the context.
        self.context.own_model(self)

        self.clk = clk
        self.rst = rst
        self.s_axis_tdata = s_axis_tdata
        self.s_axis_tvalid = s_axis_tvalid
        self.s_axis_tready = s_axis_tready
        self.s_axis_tid = s_axis_tid
        self._buf = []
        self._tid_buf = []
        self._next_ready = 0

    @classmethod
    def init_bus(cls, clk: Signal8, rst: Signal8, s_axis: Axis):
        """"""
        return cls(clk, rst, s_axis.tdata, s_axis.tvalid, s_axis.tready, s_axis.tid)

    @property
    def ready(self):
        return self._next_ready

    @ready.setter
    def ready(self, value: int):
        self._next_ready = value

    def eval_step(self) -> None:
        """Save data into a buf when it arrives."""
        if self.clk.posedge():
            self.s_axis_tready.d = self._next_ready
            if self.s_axis_tvalid.q and self.s_axis_tready.q:
                self._buf.append(self.s_axis_tdata.q)
                if self.tid:
                    self._tid_buf.append(self.tid.q)

    def read(
        self,
        clear: bool = True,
        float_q: int = None,
        sign_extend: int = None,
        tid: bool = False,
    ) -> _ArrayLike:
        """"""
        _dt = _np.double if float_q else _np.int64
        res = _np.array(self._buf.copy()).astype(_dt)
        tid_res = self._tid_buf.copy()
        if clear:
            self._buf.clear()
            self._tid_buf.clear()

        if float_q:
            qm = 1.0 / (2**float_q)
            res *= qm

        if tid:
            return (res, tid_res)
        return res
