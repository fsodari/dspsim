from dspsim.framework import Context, Clock, signal, dff
from dspsim.wishbone import Wishbone, WishboneM32
from dspsim.library import BramSdp, BramTdp
from dspsim.library import WbBram
from dspsim.library import AsyncSync8
from dspsim.bram import BramBus

import numpy as np
from pathlib import Path

from random import randint

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_bram_sdp():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    # Wishbone busses
    wb0 = Wishbone()
    wb1 = Wishbone()

    # Bram busses
    bmem0 = BramBus(BramSdp.DW, BramSdp.DEPTH)
    bmem1 = BramBus(BramSdp.DW, BramSdp.DEPTH)

    # Simple Dual Port Ram.
    bram = BramSdp(clk, rst, *bmem0, *bmem1)

    # Wishbone Bram controllers. Wishbone slave interface to bram master interface.
    wb_bram0 = WbBram(clk, rst, *wb0, *bmem0)
    wb_bram1 = WbBram(clk, rst, *wb1, *bmem1)

    # Wishbone masters to drive the WbBram controllers from the test bench.
    wbm0 = WishboneM32(clk, rst, *wb0)
    wbm1 = WishboneM32(clk, rst, *wb1)

    bram.trace(trace_dir / "bram_sdp.vcd")

    context.elaborate()
    print(context)

    rst.d = 1
    context.run(100)
    rst.d = 0
    context.run(100)

    # Blocking write.
    write_data = [randint(1, int(1e6)) for _ in range(BramSdp.DEPTH)]
    wbm0.write(0, write_data, timeout=100000)

    # Blocking read.
    read_data = wbm1.read(list(range(BramSdp.DEPTH)), timeout=100000)
    context.run(100)

    assert np.all(read_data == write_data)


def test_bram_tdp():
    context = Context(1e-9, 1e-9)

    # Fast clock
    clka = Clock(10e-9)
    rsta = signal()

    # Slow clock
    clkb = Clock(20e-9)
    rstb = dff(clkb, 1)

    # Synchronize slow reset to fast reset clock.
    AsyncSync8(clka, rstb, rsta)

    # Wishbone busses
    wb0 = Wishbone()
    wb1 = Wishbone()

    # Bram busses
    bmem0 = BramBus(BramTdp.DW, BramTdp.DEPTH)
    bmem1 = BramBus(BramTdp.DW, BramTdp.DEPTH)

    # True Dual Port Ram
    bram = BramTdp(clka, rsta, *bmem0, clkb, rstb, *bmem1)

    # Wishbone Bram controllers. Wishbone slave interface to bram master interface.
    wb_bram0 = WbBram(clka, rsta, *wb0, *bmem0)
    wb_bram1 = WbBram(clkb, rstb, *wb1, *bmem1)

    # Wishbone masters to drive the WbBram controllers from the test bench.
    wbm0 = WishboneM32(clka, rsta, *wb0)
    wbm1 = WishboneM32(clkb, rstb, *wb1)

    bram.trace(trace_dir / "bram_tdp.vcd")

    context.elaborate()
    print(context)

    rstb.d = 1
    context.run(1000)
    rstb.d = 0
    context.run(100)

    # Write to wbm0 and read from wbm1
    write_data = [randint(0, int(1e6)) for _ in range(BramTdp.DEPTH)]
    wbm0.write(0, write_data, timeout=100000)

    read_data = wbm1.read(list(range(BramTdp.DEPTH)), timeout=100000)
    context.run(100)

    assert np.all(read_data == write_data)

    context.run(100)

    # Write to wbm1 and read from wbm0
    write_data = [randint(0, int(1e6)) for _ in range(BramTdp.DEPTH)]
    wbm1.write(0, write_data, timeout=10000000)

    read_data = wbm0.read(list(range(BramTdp.DEPTH)), timeout=100000)
    context.run(100)

    assert np.all(read_data == write_data)
