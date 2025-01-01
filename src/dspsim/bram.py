from dspsim.framework import Signal8, SignalT, signal
import numpy as np


class BramBus:
    en: Signal8
    we: Signal8
    addr: SignalT
    dout: SignalT
    din: SignalT
    regce: Signal8

    _width: int
    _depth: int
    _address_width: int

    def __init__(self, width: int = 32, depth: int = 1024):
        self._width = width
        self._depth = depth
        self._address_width = int(np.ceil(np.log2(depth)))

        self.en = signal()
        self.we = signal()
        self.addr = signal(width=self._address_width)
        self.dout = signal(width=width)
        self.din = signal(width=width)
        self.regce = signal()

    def __iter__(self) -> tuple[SignalT]:
        return iter((self.en, self.we, self.addr, self.dout, self.din, self.regce))

    def __str__(self) -> str:
        return f"BramPort(width={self.width}, depth={self.depth}, address_width={self.address_width})"

    @property
    def width(self) -> int:
        return self._width

    @property
    def depth(self) -> int:
        return self.depth

    @property
    def address_width(self) -> int:
        return self._address_width
