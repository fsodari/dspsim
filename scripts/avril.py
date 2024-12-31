from dataclasses import dataclass
import struct
import random
import serial
import serial.tools.list_ports
from contextlib import contextmanager
from cobs import cobs

import serial.win32

VID = 0x6666
PID = 0xD510


def find_device(pid: int):
    """Find the device by its pid."""
    ports = list(serial.tools.list_ports.comports())

    return [port.device for port in ports if port.pid == pid]


AVRIL_CMD_NOP = 0
AVRIL_CMD_WRITE = 1
AVRIL_CMD_READ = 2
AVRIL_CMD_NOP_ACK = 3
AVRIL_CMD_WRITE_ACK = 4
AVRIL_CMD_READ_ACK = 5


@dataclass
class AvrilMessage:
    command: int
    mode: int
    msg_id: int
    size: int
    address: int
    data: bytes = b""

    _fmt = "<BBHLL"

    def __post_init__(self):
        if self.msg_id is None:
            self.msg_id = random.randint(0, 0xFFFF)

    @classmethod
    def from_bytes(cls, b: bytes):
        return cls(*struct.unpack(cls._fmt, b[:12]), b[12:])

    def to_bytes(self) -> bytes:
        header = struct.pack(self._fmt, *list(vars(self).values())[:5])
        return header + self.data

    @classmethod
    def decode(cls, encoded: bytes):
        return cls.from_bytes(cobs.decode(encoded[:-1]))

    def encode(self) -> bytes:
        return cobs.encode(self.to_bytes()) + b"\0"


@dataclass
class AvrilAck(AvrilMessage):
    error: int = 0

    def __post_init__(self):
        super().__post_init__()
        self.error = struct.unpack("<L", self.data[:4])[0]
        self.data = self.data[4:]


import time


class Avril:
    mode: int
    pid: int
    port: str
    device: serial.Serial

    def __init__(
        self,
        mode: int,
        pid: int = 0xD510,
        timeout: float = 1.0,
        write_timeout: float = 1.0,
        inter_byte_timeout: float = None,
    ):
        self.mode = mode
        self.pid = pid
        self.port = find_device(self.pid)[0]
        self.device = serial.Serial(
            timeout=timeout,
            write_timeout=write_timeout,
            inter_byte_timeout=inter_byte_timeout,
        )

    def __enter__(self):
        time.sleep(0.2)
        self.device.port = self.port
        self.device.open()
        self.device.flush()
        return self

    def __exit__(self, *args):
        self.device.flush()
        self.device.close()
        pass

    def write(self, address: int, data: bytes, msg_id: int = None) -> AvrilAck:
        cmd = AvrilMessage(AVRIL_CMD_WRITE, self.mode, msg_id, len(data), address, data)
        # print(cmd)
        self.device.write(cmd.encode())

        expected_length = cobs.encoding_overhead(16) + 17
        response = self.device.read(expected_length)
        # print(response)
        ack = AvrilAck.decode(response)
        # print(ack)
        return ack

    def writef(self, address: int, fmt: str, *data) -> AvrilAck:
        x = struct.pack(fmt, *data)
        return self.write(address, x)

    def read(self, address: int, size: int, msg_id: int = None) -> AvrilAck:
        cmd = AvrilMessage(AVRIL_CMD_READ, self.mode, msg_id, size, address)
        # print(cmd)
        self.device.write(cmd.encode())

        expected_length = cobs.encoding_overhead(16 + size) + 17 + size
        response = self.device.read(expected_length)
        # print(f"Expected: {expected_length}, Got: {len(response)}")
        ack = AvrilAck.decode(response)
        # print(ack)
        return ack


import random
from tqdm import trange


def main():
    """"""
    # N = 12
    # with Avril(0) as av:
    #     # tx_data = bytes(range(4))
    #     tx_data = random.randbytes(N)
    #     ack = av.write(0, tx_data)

    #     ack = av.read(0, len(tx_data))
    #     assert ack.data == tx_data
    N = 229
    with Avril(0, timeout=0.1) as av:
        for i in trange(1000):
            # for i in range(200):
            _x = [random.randint(0, 0xFF) for _ in range(N)]
            # _x[120] = 0
            x = bytes(_x)

            assert cobs.decode(cobs.encode(x)) == x
            # print(x[:4])
            ack = av.write(0, x)
            assert not ack.error
            # print(ack.command)

            # ack = av.read(0, N)
            # # print(ack.command)
            # assert not ack.error
            # # print(ack)
            # assert ack.data == x


if __name__ == "__main__":
    main()
