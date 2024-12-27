from dspsim.framework import Context, Clock, dff
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.wishbone import Wishbone, WishboneM32, WishboneMU32
from dspsim.library import Mixer

import numpy as np
from pathlib import Path

# import pytest

from numpy.random import default_rng

rgen = default_rng()

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_mixer_basic():
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = dff(clk, 1)

        b0 = Axis(width=Mixer.DW, tid=True, tlast=True)
        b1 = Axis(width=Mixer.DW, tid=True, tlast=True)
        wb0 = Wishbone()

        mixer = Mixer(clk, rst, *b0, *b1, *wb0)

        DATAQ = 20

        axis_tx = AxisTx(clk, rst, b0, tid_pattern=range(Mixer.N), width=Mixer.DW)
        axis_rx = AxisRx(clk, rst, b1, width=Mixer.DW)
        wbm = WishboneMU32(clk, rst, *wb0)

        mixer.trace(trace_dir / "mixer.vcd")

        context.elaborate()

        rst.d = 1
        context.run(100)
        rst.d = 0
        context.run(100)

        # Random coefficient matrix
        coefs = rgen.uniform(-0.5, 0.5, size=(Mixer.M, Mixer.N))
        _coefs = (coefs * 2**Mixer.COEFQ).flatten()
        # Write coefs
        wbm.write(0, _coefs.astype(np.uint32))

        axis_rx.tready = True

        # Send multiple data sets.
        NT = 40
        tx_data = rgen.uniform(-1.0, 1.0, size=(NT, Mixer.N))

        # Queue up all of the tx data.
        for x in tx_data:
            axis_tx.write_command(x, q=DATAQ)

        for x in tx_data:
            rx_data = np.array(axis_rx.read(n=Mixer.M)).astype(np.int32) * 2**-DATAQ
            # Compare the rx data to the expected result.
            y = coefs @ x

            assert np.all(np.isclose(rx_data, y, atol=0.0001))

        context.run(100)
