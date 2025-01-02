from dspsim.framework import Signal8, SignalT, signal
from dspsim._framework import AxilM32

class Axil:
    awaddr: SignalT
    awvalid: Signal8
    awready: Signal8
    wdata: SignalT
    wvalid: Signal8
    wready: Signal8
    bresp: Signal8
    bvalid: Signal8
    bready: Signal8
    araddr: SignalT
    arvalid: Signal8
    arready: Signal8
    rdata: SignalT
    rresp: Signal8
    rvalid: Signal8
    rready: Signal8

    def __init__(self, address_width: int = 32, data_width: int = 32):
        self.awaddr = signal(width=address_width)
        self.awvalid = signal()
        self.awready = signal()
        self.wdata = signal(width=data_width)
        self.wvalid = signal()
        self.wready = signal()
        self.bresp = signal(width=2)
        self.bvalid = signal()
        self.bready = signal()
        self.araddr = signal(width=address_width)
        self.arvalid = signal()
        self.arready = signal()
        self.rdata = signal(width=data_width)
        self.rresp = signal(width=2)
        self.rvalid = signal()
        self.rready = signal()

    @property
    def address_width(self) -> int:
        return self.awaddr.width

    @property
    def data_width(self) -> int:
        return self.wdata.width

    def __iter__(self):
        return iter(
            (
                self.awaddr,
                self.awvalid,
                self.awready,
                self.wdata,
                self.wvalid,
                self.wready,
                self.bresp,
                self.bvalid,
                self.bready,
                self.araddr,
                self.arvalid,
                self.arready,
                self.rdata,
                self.rresp,
                self.rvalid,
                self.rready,
            )
        )

__all__ = [
    "Axil",
    "AxilM32"
]