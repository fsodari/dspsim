from dspsim.framework import Context, Clock, signal, dff
from dspsim.wishbone import Wishbone, WishboneM32
from dspsim.library import WbRegs32
import numpy as np
from pathlib import Path

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_wishbone_regs():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    wb = Wishbone()

    ctl_regs = signal(width=WbRegs32.CFGDW, shape=(WbRegs32.N_CTL,))
    sts_regs = signal(width=WbRegs32.CFGDW, shape=(WbRegs32.N_STS,))

    wbm = WishboneM32(clk, rst, *wb)
    wb_regs = WbRegs32(clk, rst, *wb, ctl_regs, sts_regs)

    wb_regs.trace(trace_dir / "wb_regs.vcd")

    context.elaborate()
    print(context.print_info())

    rst.d = 1
    context.advance(100)
    rst.d = 0
    context.advance(100)

    # Send tx data as dict.
    tx_data = {i: i for i in range(WbRegs32.N_CTL)}
    # Blocking write.
    wbm.write(tx_data)
    # Blocking read.
    rx_data = wbm.read(list(tx_data.keys()))
    assert np.all(rx_data == list(tx_data.values()))

    # Check that the registers match.
    ctl_vals = [s.q for s in ctl_regs]
    assert np.all(ctl_vals == list(tx_data.values()))

    # __getitem__, __setitem__ interfaace. blocking.
    wbm[12] = 42
    x = wbm[12]
    assert x == 42
    assert ctl_regs[12].q == 42

    context.advance(100)
