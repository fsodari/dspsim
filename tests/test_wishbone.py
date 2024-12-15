from dspsim.framework import Context, Clock, signal, dff, SignalT, Signal8, Model
from dspsim.wishbone import Wishbone, WishboneM32
from dspsim.library import WbRegs32

import numpy as np


def test_wishbone_regs():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    wb = Wishbone()

    ctl_regs = signal(width=WbRegs32.CFGDW, shape=(WbRegs32.N_CTL,))
    sts_regs = signal(width=WbRegs32.CFGDW, shape=(WbRegs32.N_STS,))

    wbm = WishboneM32(clk, rst, *wb)
    wb_regs = WbRegs32(clk, rst, *wb, ctl_regs, sts_regs)

    wb_regs.trace("traces/wb_regs.vcd")

    context.elaborate()
    print(context.print_info())

    rst.d = 1
    context.advance(100)
    rst.d = 0
    context.advance(100)

    wbm.write(0, range(32))

    context.advance(1000)

    wbm[12] = 42
    x = wbm[12]
    assert x == 42
    assert ctl_regs[12].q == 42

    context.advance(100)
