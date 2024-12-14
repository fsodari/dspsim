from dspsim.framework import Context, Clock, signal, dff
from dspsim.library import AsyncSync8
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.fifo import FifoAsync

from dspsim.i2s import I2S, I2SClkGen, I2STx, I2SRx
import numpy as np


# @framework.runner(time_unit=1e-9, time_precision=1e-9)
def test_i2s_loopback():
    with Context(1e-9, 1e-9) as context:
        fs = 48e3
        fs_mclk = fs * 384
        T_mclk = 1 / fs_mclk  # About 54ns

        # Main bus clock.
        bclk = Clock(10e-9)
        # Audio clock.
        mclk = Clock(T_mclk)

        rst = dff(bclk, 1)
        irst = signal()
        # Synchronize the master reset with the i2s clk.
        AsyncSync8(mclk, rst, irst)

        # i2s bus
        i2s = I2S(mclk)
        # If the fpga is generating the i2s clock, use the clkgen component.
        i2s_clk_gen = I2SClkGen.init_bus(mclk, irst, i2s)

        tx_data_bus = Axis(width=I2STx.DW, tid=True)
        tx_fifo_bus = Axis(width=I2STx.DW, tid=True)
        i2s_rx_bus = Axis(width=I2SRx.DW, tid=True)
        rx_fifo_bus = Axis(width=I2SRx.DW, tid=True)

        # Send data to the fifo component. Alternates tid.
        axis_tx = AxisTx(bclk, rst, tx_data_bus, tid_pattern=[1, 0], width=I2STx.DW)

        # Cross clock domains with an async fifo.
        fifo_tx = FifoAsync.init_bus(bclk, rst, tx_data_bus, mclk, irst, tx_fifo_bus)

        # I2S component receives data from fifo, then sends it on the i2s bus.
        i2s_tx = I2STx.init_bus(mclk, irst, tx_fifo_bus, i2s)

        # I2S component receives i2s data, then sends it to a fifo.
        i2s_rx = I2SRx.init_bus(mclk, irst, i2s_rx_bus, i2s)

        # Fifo crosses clock domains back to bus clock.
        fifo_rx = FifoAsync.init_bus(mclk, irst, i2s_rx_bus, bclk, rst, rx_fifo_bus)

        # Receive axis data.
        axis_rx = AxisRx(bclk, rst, rx_fifo_bus)

        print(context.print_info())

        i2s_clk_gen.trace("traces/i2s_clk_gen.vcd")
        i2s_tx.trace("traces/i2s_tx.vcd")
        i2s_rx.trace("traces/i2s_rx.vcd")
        fifo_tx.trace("traces/i2s_fifo_tx.vcd")
        fifo_rx.trace("traces/i2s_fifo_rx.vcd")

        context.elaborate()

        tx_data = np.linspace(1, 42, 47, dtype=np.int32)
        print(tx_data)
        axis_tx.write(tx_data)

        rst.d = 1
        context.advance(500)
        rst.d = 0
        context.advance(100)
        axis_rx.tready = True

        context.advance(1000000)

        rx_data = axis_rx.read()
        print(rx_data)
        # print(rx_tid)
        # assert all(rx_data == tx_data)
