from dspsim._framework import WishboneM8, WishboneM16, WishboneM32, WishboneM64
from dspsim._framework import Signal8, Signal16, Signal32, Signal64

from dspsim.framework import SignalT, signal


class Wishbone:
    cyc: Signal8
    stb: Signal8
    we: Signal8
    ack: Signal8
    stall: Signal8
    addr: SignalT
    data_o: SignalT
    data_i: SignalT

    _address_width: int
    _data_width: int

    @property
    def data_width(self) -> int:
        return self._data_width

    @property
    def address_width(self) -> int:
        return self._address_width

    def __init__(self, address_width: int = 32, data_width: int = 32):
        self._address_width = address_width
        self._data_width = data_width

        self.cyc = Signal8()
        self.stb = Signal8()
        self.we = Signal8()
        self.ack = Signal8()
        self.stall = Signal8()
        self.addr = signal(width=address_width)
        self.data_o = signal(width=data_width)
        self.data_i = signal(width=data_width)

    def __str__(self):
        return f"Wishbone(address_width={self.address_width}, data_width={self.data_width})"

    def __iter__(self):
        return iter(
            (
                self.cyc,
                self.stb,
                self.we,
                self.ack,
                self.stall,
                self.addr,
                self.data_o,
                self.data_i,
            )
        )
