from dataclasses import dataclass
import struct
import random
import serial
import serial.tools.list_ports
from contextlib import contextmanager

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
class AvrilHeader:
    command: int
    mode: int
    msg_id: int
    size: int
    address: int

    _fmt = "<BBHLL"

    @classmethod
    def from_bytes(cls, b: bytes):
        return cls(*struct.unpack(cls._fmt, b))

    def to_bytes(self) -> bytes:
        return struct.pack(self._fmt, *vars(self).values())


@dataclass
class AvrilAck(AvrilHeader):
    error: int
    data: bytes = b""

    _fmt = "<BBHLLL"

    @classmethod
    def from_bytes(cls, b: bytes):
        return cls(*struct.unpack(cls._fmt, b[:16]), b[16:])

    def to_bytes(self):
        return struct.pack(self._fmt, *list(vars(self).values())[:-1]) + self.data


def write_cmd(address: int, data: bytes, mode: int = 0, msg_id: int = None):
    """"""
    if msg_id is None:
        msg_id = random.randint(0, 0xFFFF)
    return AvrilHeader(AVRIL_CMD_WRITE, mode, msg_id, len(data), address)


def read_cmd(address: int, size: int, mode: int = 0, msg_id: int = None):
    """"""
    if msg_id is None:
        msg_id = random.randint(0, 0xFFFF)
    return AvrilHeader(AVRIL_CMD_READ, mode, msg_id, 0, address)


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
    ):
        self.mode = mode
        self.pid = pid
        self.port = find_device(self.pid)[0]
        self.device = serial.Serial(timeout=timeout, write_timeout=write_timeout)

    def __enter__(self):
        self.device.port = self.port
        self.device.open()
        self.device.flush()
        return self

    def __exit__(self, *args):
        self.device.close()
        pass

    def write(self, address: int, data: bytes, msg_id: int = None) -> AvrilAck:
        cmd = write_cmd(address, data, self.mode, msg_id=msg_id)
        self.device.write(cmd.to_bytes() + data)
        return AvrilAck.from_bytes(self.device.read(16))

    def writef(self, address: int, fmt: str, *data) -> AvrilAck:
        x = struct.pack(fmt, *data)
        return self.write(address, x)

    def read(self, address: int, size: int, msg_id: int = None) -> AvrilAck:
        cmd = read_cmd(address, size, self.mode, msg_id=msg_id)
        self.device.write(cmd.to_bytes())

        response = self.device.read(16 + size)
        return AvrilAck.from_bytes(response)


import random
from tqdm import trange


def main():
    """"""
    with Avril(0) as av:
        for i in trange(1000):
            x = random.randbytes(4)
            ack = av.write(0, x)
            assert not ack.error

            ack = av.read(0, 4)
            assert not ack.error
            assert ack.data == x


if __name__ == "__main__":
    main()
