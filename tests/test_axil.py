from dspsim.framework import Context, Clock, signal, dff
from dspsim.axil import Axil, AxilM32
from dspsim.library import AxilRegs
import numpy as np
from pathlib import Path

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_axil_regs():
    context = Context(1e-9, 1e-9)

    clk = Clock(10e-9)
    rst = dff(clk, 1)

    al0 = Axil()

    ctl_regs = signal(width=AxilRegs.CFGDW, shape=(AxilRegs.N_CTL,))
    sts_regs = signal(width=AxilRegs.CFGDW, shape=(AxilRegs.N_STS,))

    axil_m = AxilM32(clk, rst, *al0)
    axil_regs = AxilRegs(clk, rst, *al0, ctl_regs, sts_regs)

    axil_regs.trace(trace_dir / "axil_regs.vcd")

    context.elaborate()
    print(context)

    rst.d = 1
    context.run(100)
    rst.d = 0
    context.run(100)

    # Send tx data as dict.
    tx_data = list(range(AxilRegs.N_CTL))
    # Blocking write.
    axil_m.write(0, tx_data)

    # Blocking read.
    rx_data, rx_resp = axil_m.read(list(range(AxilRegs.N_CTL)))
    assert np.all(rx_data == tx_data)

    # Check that the registers match.
    ctl_vals = [s.q for s in ctl_regs]
    assert np.all(ctl_vals == tx_data)

    # # __getitem__, __setitem__ interfaace. blocking.
    axil_m[12] = 42
    x = axil_m[12]
    assert x == 42
    assert ctl_regs[12].q == 42

    context.run(100)
