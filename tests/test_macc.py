from dspsim.framework import Context, Clock, signal, dff
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.library import macc_core, Macc
from dspsim.util import to_fixed, to_float, sign_extendv

import numpy as np
from pathlib import Path

from matplotlib import pyplot as plt
import seaborn as sns

sns.set_theme()

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_macc_basic():
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = dff(clk, 1)

        adata = Axis(width=Macc.ADW, tlast=True)
        bdata = Axis(width=Macc.BDW)
        accum_data = Axis(width=Macc.ODW)

        macc = Macc(clk, rst, *adata, *bdata, *accum_data)

        axis_a_tx = AxisTx(clk, rst, adata, width=Macc.ADW)
        axis_b_tx = AxisTx(clk, rst, bdata, width=Macc.BDW)
        axis_rx = AxisRx(clk, rst, accum_data, width=Macc.ODW)

        macc.trace(trace_dir / "Macc.vcd")

        context.elaborate()
        print(context)

        rst.d = 1
        context.run(100)
        rst.d = 0
        context.run(100)

        DATAQ = 22
        COEFQ = 16
        OUTPUTQ = DATAQ + COEFQ

        # a_tx_data = np.linspace(0, 1, 10) * 2**DATAQ
        a_tx_data = np.array(10 * [1.0]) * 2**DATAQ
        b_tx_data = np.array(10 * [0.5]) * 2**COEFQ

        axis_rx.tready = True

        axis_a_tx.write_command(a_tx_data)
        axis_b_tx.write_command(b_tx_data)

        rx_data = axis_rx.read(timeout=10000) >> OUTPUTQ
        assert rx_data == 5

        axis_a_tx.write_command(a_tx_data)
        axis_b_tx.write_command(b_tx_data * 2)

        rx_data = axis_rx.read(timeout=10000) >> OUTPUTQ
        assert rx_data == 10

        context.run(100)
