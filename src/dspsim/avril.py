from dataclasses import dataclass
import struct
import random

from cobs import cobs
from dataclass_wizard import YAMLWizard
from pathlib import Path
import sys

import serial
import serial.tools.list_ports

if sys.platform == "win32":
    import serial.win32

# Default VID/PID
VID = 0x6666

PID = 0xD510


def find_device(pid: int):
    """Find the serial port by its pid."""
    ports = list(serial.tools.list_ports.comports())

    return [port.device for port in ports if port.pid == pid]


AVRIL_CMD_NOP = 0
AVRIL_CMD_WRITE = 1
AVRIL_CMD_READ = 2
AVRIL_CMD_NOP_ACK = 3
AVRIL_CMD_WRITE_ACK = 4
AVRIL_CMD_READ_ACK = 5

AVRIL_MODE_VMMI = 0
AVRIL_MODE_BOOTLOAD = 1
AVRIL_MODE_META = 2

# Decode the integer dtype key to the struct.unpack/pack dtype format.
dtype_decode: dict[int, str] = {
    0: "x",
    0x1: "b",
    0x10001: "B",
    0x2: "h",
    0x10002: "H",
    0x4: "l",
    0x10004: "L",
    0x8: "q",
    0x10008: "Q",
    0x20004: "f",
    0x20008: "d",
}


def dtype_size(dtype: str) -> int:
    """Get the dtype size from its key. The lower 16 bits represent a size, and upper 16 are for a unique table index."""
    key = list(dtype_decode.keys())[list(dtype_decode.values()).index(dtype)]
    return abs(key) & 0xFFFF


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


@dataclass
class VMetaEntry:
    base_address: int
    size: int
    dtype: str
    name: str

    @classmethod
    def unpack(cls, b: bytes):
        dtype_offset = 8
        name_offset = 12
        return cls(
            *struct.unpack("<LL", b[:dtype_offset]),
            dtype_decode[struct.unpack("<L", b[dtype_offset:name_offset])[0]],
            b[name_offset:].decode().strip("\0"),
        )


@dataclass
class VReg:
    address: int
    dtype: str = None
    default: int | float = 0
    description: str = ""


@dataclass
class VIFace:
    base_address: int
    size: int
    registers: dict[str, VReg]


@dataclass
class VRegMap(YAMLWizard):
    interfaces: dict[str, dict[str, VReg]]


class Avril:
    mode: int
    pid: int
    port: str
    device: serial.Serial
    chunk_size: int = 48

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
        self.chunk_size = 44
        self.meta_address = 0

    def __enter__(self):
        time.sleep(0.2)
        self.device.port = self.port
        self.device.open()
        self.device.flush()
        return self

    def __exit__(self, *args):
        self.device.flush()
        self.device.close()

    def _serial_send(self, b: bytes, chunk: bool = True):
        """"""
        chunk_size = self.chunk_size if chunk else len(b)
        remain = len(b)
        while remain > 0:
            ssize = chunk_size if remain > chunk_size else remain
            self.device.write(b[:ssize])
            remain -= ssize
            b = b[ssize:]

    def _serial_read(self, size: int, chunk: bool = True):
        """"""
        chunk_size = self.chunk_size if chunk else size
        b = b""
        while size > 0:
            ssize = chunk_size if size > chunk_size else size
            b += self.device.read(ssize)
            size -= ssize
        return b

    def write(
        self, address: int, data: bytes, msg_id: int = None, chunk: bool = True
    ) -> AvrilAck:
        cmd = AvrilMessage(AVRIL_CMD_WRITE, self.mode, msg_id, len(data), address, data)
        self._serial_send(cmd.encode(), chunk=chunk)
        response = self._serial_read(cobs.max_encoded_length(16) + 1)
        return AvrilAck.decode(response)

    def writef(self, address: int, fmt: str, *data) -> AvrilAck:
        x = struct.pack(fmt, *data)
        return self.write(address, x)

    def read(
        self, address: int, size: int, msg_id: int = None, chunk: bool = True
    ) -> AvrilAck:
        cmd = AvrilMessage(AVRIL_CMD_READ, self.mode, msg_id, size, address)
        self._serial_send(cmd.encode())

        expected_length = cobs.max_encoded_length(16 + size) + 1
        response = self._serial_read(expected_length, chunk)
        return AvrilAck.decode(response)

    def read_meta(self, id: int, mode: int = None) -> VMetaEntry:
        if mode is None:
            mode = AVRIL_MODE_META
        addr = id * 28
        cmd = AvrilMessage(AVRIL_CMD_READ, mode, None, 28, addr)
        self._serial_send(cmd.encode())

        expected_length = cobs.max_encoded_length(16 + 28) + 1
        response = self._serial_read(expected_length)

        ack = AvrilAck.decode(response)
        return VMetaEntry.unpack(ack.data)

    def read_all_meta(self, mode: int = None) -> dict[str, VMetaEntry]:
        """"""
        all_meta = {}
        max = int(4096 / 28)
        for i in range(max):
            try:
                entry = self.read_meta(i, mode)
                all_meta[entry.name] = entry
            except Exception as e:
                print(f"{e}")
                break
        return all_meta

    def get_interface(self, interface: str, registers: dict[str, int] = {}):
        """"""
        return AvrilIface(self, interface, registers)


class AvrilIface:
    av: Avril
    interface: str
    meta: VMetaEntry
    dtype: str
    registers: dict[str, VReg]

    def __init__(self, av: Avril, interface: str, registers: dict[str, VReg] = {}):
        self.av = av
        self.interface = interface
        self.meta = self.av.read_all_meta()[interface]
        self.dtype = self.meta.dtype
        self.load_registers(registers)

    def load_registers(self, registers: dict[str, VReg]):
        self.registers = registers
        for rname, r in self.registers.items():
            if r.dtype is None:
                r.dtype = self.meta.dtype

    def load_register_file(self, regmap: Path):
        """"""
        iface_cfg = VRegMap.from_yaml_file(regmap)
        self.load_registers(iface_cfg.interfaces[self.interface])

    def __str__(self):
        return f"AvrilIface(av={self.av}, meta={self.meta})"

    def _get_address(self, address: int | str) -> int:
        # If the address is a string, look up the address in the registers
        if isinstance(address, str):
            address = self.registers[address].address
        return address + self.base_address

    def write(self, _address: int | str, data: bytes):
        address = self._get_address(_address)
        ack = self.av.write(address, data)
        return ack

    def read(self, _address: int | str, size: int):
        address = self._get_address(_address)
        return self.av.read(address, size)

    def write_reg(self, address: int | str, data: int | float, dtype: str = None):
        """"""
        if dtype is None:
            dtype = self.dtype

        bdata = struct.pack(f"<{dtype}", data)
        return self.write(address, bdata)

    def read_reg(self, address: int | str, dtype: str = None) -> int | float:
        if dtype is None:
            dtype = self.dtype
        ack = self.read(address, self.dtype_size)
        return struct.unpack(f"<{dtype}", ack.data)[0]

    def __getitem__(self, address: int | str) -> int | float:
        return self.read_reg(address)

    def __setitem__(self, address: int | str, data: int | float):
        self.write_reg(address, data)

    @property
    def size(self) -> int:
        return self.meta.size

    @property
    def dtype_size(self) -> int:
        return dtype_size(self.dtype)

    @property
    def base_address(self) -> int:
        return self.meta.base_address

    def __iter__(self):
        return iter(range(0, self.size, self.dtype_size))
