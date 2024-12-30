import serial
import serial.tools.list_ports

from contextlib import contextmanager

VID = 0x6666
PID = 0xD510


def find_device(pid: int):
    """Find the device by its pid."""
    ports = list(serial.tools.list_ports.comports())

    return [port.device for port in ports if port.pid == pid]


@contextmanager
def open_serial_port(pid, **kwargs):
    """"""
    port = find_device(pid)[0]
    dev = serial.Serial(port, **kwargs)
    try:
        # dev.open()
        dev.flush()
        yield dev
    finally:
        dev.close()


import time
from random import randbytes
from tqdm import trange


def main():
    device = find_device(PID)[0]
    print(device)

    MSG_SIZE = 5000
    N = 2000
    with open_serial_port(PID, timeout=1.0, write_timeout=1.0) as dev:
        for i in trange(N):
            tx_data = randbytes(MSG_SIZE)
            dev.write(tx_data)

            rx_data = dev.read(len(tx_data))
            assert rx_data == tx_data


if __name__ == "__main__":
    main()
