from dspsim.framework import Context, Clock, dff
from dspsim.axis import Axis, AxisTx32, AxisRx32
from dspsim.wishbone import Wishbone, WishboneM32
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

        b0 = Axis(width=Mixer.DW, signed=True, tid=True, tlast=True)
        b1 = Axis(width=Mixer.DW, signed=True, tid=True, tlast=True)
        wb0 = Wishbone(signed=True)

        mixer = Mixer(clk, rst, *b0, *b1, *wb0)

        axis_tx = AxisTx32(clk, rst, *b0, tid_pattern=range(Mixer.N))
        axis_rx = AxisRx32(clk, rst, *b1)
        wbm = WishboneM32(clk, rst, *wb0)

        mixer.trace(trace_dir / "mixer.vcd")
        context.elaborate()

        rst.d = 1
        context.run(100)
        rst.d = 0
        context.run(100)

        # Random coefficient matrix
        coefs = rgen.uniform(-0.5, 0.5, size=(Mixer.M, Mixer.N))
        # Write coefs
        wbm.write(0, coefs.flatten(), q=Mixer.COEFQ)

        axis_rx.tready = True

        # Send multiple data sets.
        NT = 40
        tx_data = rgen.uniform(-1.0, 1.0, size=(NT, Mixer.N))

        # Queue up all of the tx data.
        DATAQ = 22
        for x in tx_data:
            axis_tx.write_command(x, q=DATAQ)

        # Read it out all operations and compare results.
        for x in tx_data:
            rx_data = axis_rx.read(n=Mixer.M, q=DATAQ)
            # Compare the rx data to the expected result.
            y = coefs @ x
            # print(f"A: {coefs}")
            # print(f"x: {x}")
            # print(f"rx: {rx_data}")
            # print(f"y: {y}")

            assert np.all(np.isclose(rx_data, y, atol=0.0001))

        context.run(100)
