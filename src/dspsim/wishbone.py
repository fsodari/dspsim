from dspsim._framework import WishboneM8, WishboneM16, WishboneM32, WishboneM64
from dspsim._framework import WishboneMU8, WishboneMU16, WishboneMU32, WishboneMU64
from dspsim._framework import SignalU8

from dspsim.framework import SignalT, signal


class Wishbone:
    cyc: SignalU8
    stb: SignalU8
    we: SignalU8
    ack: SignalU8
    stall: SignalU8
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

    def __init__(
        self, address_width: int = 32, data_width: int = 32, signed: bool = False
    ):
        self._address_width = address_width
        self._data_width = data_width

        self.cyc = SignalU8()
        self.stb = SignalU8()
        self.we = SignalU8()
        self.ack = SignalU8()
        self.stall = SignalU8()
        self.addr = signal(width=address_width)
        self.data_o = signal(width=data_width, signed=signed)
        self.data_i = signal(width=data_width, signed=signed)

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
