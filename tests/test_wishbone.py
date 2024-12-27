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
    print(context)

    rst.d = 1
    context.run(100)
    rst.d = 0
    context.run(100)

    # Send tx data as dict.
    tx_data = list(range(WbRegs32.N_CTL))
    # Blocking write.
    wbm.write(0, tx_data)

    # Blocking read.
    rx_data = wbm.read(list(range(WbRegs32.N_CTL)))
    assert np.all(rx_data == tx_data)

    # Check that the registers match.
    ctl_vals = [s.q for s in ctl_regs]
    assert np.all(ctl_vals == tx_data)

    # __getitem__, __setitem__ interfaace. blocking.
    wbm[12] = 42
    x = wbm[12]
    assert x == 42
    assert ctl_regs[12].q == 42

    context.run(100)
