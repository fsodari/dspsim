from dspsim.framework import Context, Clock, signal, dff
from dspsim.axis import Axis, AxisTx, AxisRx64
from dspsim.library import macc_core, Macc2
from dspsim.util import to_fixed, to_float, sign_extendv

import numpy as np
from pathlib import Path

from matplotlib import pyplot as plt
import seaborn as sns

import pytest

sns.set_theme()

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_macc_basic():
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = dff(clk, 1)

        adata = Axis(width=Macc2.ADW, signed=True, tlast=True)
        bdata = Axis(width=Macc2.BDW, signed=True, tlast=True)
        accum_data = Axis(width=Macc2.ODW, signed=True)

        macc = Macc2(clk, rst, *adata, *bdata, *accum_data)

        FRAME_SIZE = 10
        axis_a_tx = AxisTx(
            clk, rst, adata, tid_pattern=range(FRAME_SIZE), width=Macc2.ADW
        )
        axis_b_tx = AxisTx(clk, rst, bdata, width=Macc2.BDW)
        axis_rx = AxisRx64(clk, rst, *accum_data)

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

        a_tx_data = np.array(FRAME_SIZE * [1.0])
        b_tx_data = np.array(FRAME_SIZE * [0.5])

        axis_rx.tready = True

        axis_a_tx.write_command(a_tx_data, q=DATAQ)
        axis_b_tx.write_command(b_tx_data, q=COEFQ)

        axis_a_tx.write_command(a_tx_data, q=DATAQ)
        axis_b_tx.write_command(b_tx_data * 2, q=COEFQ)

        rx_data = axis_rx.read(n=2, q=OUTPUTQ)

        # assert rx_data == 10
        assert np.all(rx_data == [5, 10])

        context.run(100)
