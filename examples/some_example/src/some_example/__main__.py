"""

Example demonstrating using the SomeExample component in a simulation.

"""

from dspsim.framework import Context, Clock, Dff8
from dspsim.axis import Axis, AxisTx, AxisRx
from dspsim.library import Skid
import numpy as np

from some_example import SomeExample

from pathlib import Path

trace_dir = Path("traces")
trace_dir.mkdir(exist_ok=True)


def main():
    print("Testing SomeExample Component...")

    # Create a new context.
    with Context(1e-9, 1e-9) as context:
        clk = Clock(10e-9)
        rst = Dff8(clk, 1)

        # Connect models with Axis busses.
        b0 = Axis(width=Skid.DW)
        b1 = Axis(width=Skid.DW)
        b2 = Axis(width=Skid.DW)

        # Send data onto an Axis bus
        axis_tx = AxisTx(clk, rst, b0)

        # Skid buffer
        skid = Skid(clk, rst, *b0, *b1)
        # SomeExample component, which just instantiates Skid
        some_example = SomeExample(clk, rst, *b1, *b2)

        # Receive data from Axis bus.
        axis_rx = AxisRx(clk, rst, b2)

        # Trace the skid and some_example modules.
        skid.trace("traces/skid.vcd")
        some_example.trace("traces/some_example.vcd")

        # Elaboration finishes the simulation.
        context.elaborate()
        # Display the context's information.
        print(context.print_info())

        # Start by resetting the simulation.
        rst.d = 1
        context.advance(100)
        rst.d = 0
        context.advance(100)

        # Send some data.
        tx_data = list(range(1, 6))
        axis_tx.write(tx_data)

        #
        context.advance(100)

        # Allow receiving data.
        axis_rx.tready = True
        context.advance(100)

        # Read out the data and compare it matches what was sent.
        rx_data = axis_rx.read()
        assert np.all(tx_data == rx_data)

        print("Success.")


if __name__ == "__main__":
    main()
