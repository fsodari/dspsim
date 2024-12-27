from dspsim.framework import Context, Clock, signal, dff
from dspsim.axis import Axis, AxisTx32, AxisRx32
from dspsim.wishbone import Wishbone, WishboneM32
from dspsim.library import iir_core, IIR2, IIR4, IIR6, IIR8


import numpy as np
from pathlib import Path

from matplotlib import pyplot as plt
import seaborn as sns

from scipy.signal import chirp
from scipy.fft import fft, fftfreq
import pytest

from scipy.signal import iirfilter


sns.set_theme()

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def test_iir_basic():
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = dff(clk, 1)

        b0 = Axis(width=iir_core.DW, signed=True)
        b1 = Axis(width=iir_core.DW, signed=True)

        NCOEF = int(2 * (iir_core.ORDER + 1))
        coefs = signal(width=iir_core.COEFW, signed=True, shape=(NCOEF,))

        iir = iir_core(clk, rst, *b0, *b1, coefs)

        axis_tx = AxisTx32(clk, rst, *b0)
        axis_rx = AxisRx32(clk, rst, *b1)

        iir.trace(trace_dir / "iir_core.vcd")
        context.elaborate()

        rst.d = 1
        context.run(100)
        rst.d = 0
        context.run(100)

        # Setting the first numerator coefficient to 1.0, and the rest to 0 will act as a pass-through.
        # Setting different numerators will introduce latency.
        coefs[1].d = 1 << iir_core.COEFQ

        DATAQ = 22
        NS = 10000
        fs = 48e3
        Tmax = NS / fs

        t = np.linspace(0, Tmax, NS)

        fsig = 10e3
        tx_data = np.cos(2 * np.pi * fsig * t)

        print(len(tx_data))

        axis_tx.write_command(tx_data, q=DATAQ)
        context.run(600)
        axis_rx.tready = True
        context.run(200)
        axis_rx.tready = False
        context.run(800)
        axis_rx.tready = True

        # Blocking read.
        rx_data = axis_rx.read(len(tx_data), q=DATAQ)

        assert np.all(np.isclose(rx_data[1:], tx_data[:-1], atol=0.0001))


def test_iir_chirp():
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = dff(clk, 1)

        IIR = IIR4

        b0 = Axis(width=IIR.DW, signed=True)
        b1 = Axis(width=IIR.DW, signed=True)
        wb0 = Wishbone(signed=True)

        fs = 48e3
        fc = [1e3, 10e3]

        sos: np.ndarray = iirfilter(
            IIR.ORDER // 2,
            fc,
            btype="bandpass",
            ftype="butter",
            output="sos",
            fs=fs,
        )

        NS = 100000
        DATAQ = 22

        Tmax = NS / fs
        t = np.linspace(0, Tmax, NS)

        f0 = 20
        f1 = 20e3

        tx_data = chirp(t, f0, Tmax, f1)

        iir = IIR(clk, rst, *b0, *b1, *wb0)

        axis_tx = AxisTx32(clk, rst, *b0)
        axis_rx = AxisRx32(clk, rst, *b1)

        wbm = WishboneM32(clk, rst, *wb0)

        iir.trace(trace_dir / "iir_chirp.vcd")
        context.elaborate()

        rst.d = 1
        context.run(100)
        rst.d = 0
        context.run(100)

        # Write the coefficients.
        wbm.write(0, sos.flatten(), q=IIR.COEFQ)

        axis_tx.write_command(tx_data, DATAQ)
        context.run(600)
        axis_rx.tready = True
        context.run(200)
        axis_rx.tready = False
        context.run(800)
        axis_rx.tready = True
        # # Blocking read.
        # _rx_data = axis_rx.read(len(tx_data), timeout=80000000)

        # # print(_rx_data)
        # rx_data = to_float(sign_extendv(_rx_data, IIR.DW), DATAQ)
        rx_data = axis_rx.read(len(tx_data), q=DATAQ)

        assert len(tx_data) == len(rx_data)

        # FFT
        f = fftfreq(len(tx_data), 1 / fs)[: NS // 2]

        xf = fft(tx_data)[: NS // 2]
        yf = fft(rx_data)[: NS // 2]

        hf = yf / xf

        # plt.plot(f, np.abs(xf))
        # plt.plot(f, np.abs(yf))

        # plt.semilogx(f, 20 * np.log10(np.abs(hf)))
        # plt.show()
