from dspsim.framework import Context, Clock, signal, dff
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.library import iir_core
from dspsim.util import to_fixed, to_float, sign_extendv

import numpy as np
from pathlib import Path

from matplotlib import pyplot as plt
import seaborn as sns

sns.set_theme()

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_iir_basic():
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = dff(clk, 1)

        b0 = Axis(width=iir_core.DW)
        b1 = Axis(width=iir_core.DW)

        N = int(2 * (iir_core.ORDER + 1))
        coefs = signal(width=iir_core.COEFW, shape=(N,))

        iir = iir_core(clk, rst, *b0, *b1, coefs)

        axis_tx = AxisTx(clk, rst, b0)
        axis_rx = AxisRx(clk, rst, b1)

        iir.trace(trace_dir / "iir_core.vcd")

        context.elaborate()
        print(context.print_info())

        context.advance(100)

        rst.d = 0
        context.advance(100)

        # Setting the first numerator coefficient to 1.0, and the rest to 0 will act as a pass-through.
        # Setting different numerators will introduce latency.
        coefs[1].d = int(to_fixed(1.0, iir_core.COEFQ))

        Tbus = 10e-9
        Tmax = 1e-6

        t = np.linspace(0, Tmax, int(Tmax / Tbus))
        fsig = 1e6
        DATAQ = 22
        tx_data = np.cos(2 * np.pi * fsig * t)
        print(len(tx_data))

        axis_tx.write_command(tx_data, DATAQ)
        context.advance(600)
        axis_rx.tready = True
        context.advance(200)
        axis_rx.tready = False
        context.advance(800)
        axis_rx.tready = True
        # Blocking read.
        _rx_data = axis_rx.read(len(tx_data), timeout=2000000)

        # print(_rx_data)
        rx_data = to_float(sign_extendv(_rx_data, iir_core.DW), DATAQ)

        assert np.all(np.isclose(rx_data[1:], tx_data[:-1], rtol=0.00001))

        # plt.plot(t, tx_data)
        # plt.plot(t, rx_data)
        # plt.plot(t, rx_data - tx_data)
        # plt.show()
