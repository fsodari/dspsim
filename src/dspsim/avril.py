from dataclasses import dataclass
import struct
import random

from cobs import cobs
from dataclass_wizard import YAMLWizard
from pathlib import Path
import sys

import serial
import serial.tools.list_ports

from dspsim.cutil import ErrorCode, DType, AvrilCommand, AvrilMode
from typing import Sequence, Iterable
import time

if sys.platform == "win32":
    import serial.win32

# Default VID/PID
VID = 0x6666

# USBBridge PID
BRIDGE_PID = 0xD510

# USBBootloader PID
BOOTLOAD_PID = 0xD511

# Headphone DAC PID
_HP_DAC_PID = 0xD512


def find_device(pid: int):
    """Find the serial port by its pid."""
    ports = list(serial.tools.list_ports.comports())

    return [port.device for port in ports if port.pid == pid]


@dataclass
class AvrilMessage:
    command: AvrilCommand
    mode: AvrilMode
    msg_id: int
    size: int
    address: int
    data: bytes | int | float = b""

    _fmt = "<BBHLL"

    def __post_init__(self):
        if self.msg_id is None:
            self.msg_id = random.randint(0, 0xFFFF)

    def __str__(self):
        return f"AvrilMessage(command={self.command.name}, mode={self.mode.name}, msg_id={hex(self.msg_id)}, size={self.size}, address={self.address}, data={self.data})"

    @classmethod
    def from_bytes(cls, b: bytes):
        _cmd, _mode, msg_id, size, addr = struct.unpack(cls._fmt, b[:12])
        return cls(AvrilCommand(_cmd), AvrilMode(_mode), msg_id, size, addr, b[12:])

    def to_bytes(self) -> bytes:
        _cmd = self.command.value
        _mode = self.mode.value
        header = struct.pack(
            self._fmt, _cmd, _mode, self.msg_id, self.size, self.address
        )
        return header + self.data

    @classmethod
    def decode(cls, encoded: bytes):
        return cls.from_bytes(cobs.decode(encoded[:-1]))

    def encode(self) -> bytes:
        return cobs.encode(self.to_bytes()) + b"\0"


@dataclass
class AvrilAck(AvrilMessage):
    error: ErrorCode = ErrorCode.NoError

    def __post_init__(self):
        super().__post_init__()
        self.error = ErrorCode(*struct.unpack("<L", self.data[:4]))
        self.data = self.data[4:]

    def __str__(self) -> str:
        return f"AvrilAck(command={self.command.name}, mode={self.mode.name}, msg_id={hex(self.msg_id)}, size={self.size}, address={self.address}, error={self.error.name}, data={self.data})"


@dataclass
class VMetaEntry:
    base_address: int
    size: int
    dtype: DType
    name: str

    @classmethod
    def unpack(cls, b: bytes):
        dtype_offset = 8
        name_offset = 12
        return cls(
            *struct.unpack("<LL", b[:dtype_offset]),
            DType(struct.unpack("<L", b[dtype_offset:name_offset])[0]),
            b[name_offset:].decode().strip("\0"),
        )

    def __str__(self) -> str:
        """"""
        return f"VMetaEntry(base_address={self.base_address}, size={self.size}, dtype={self.dtype.name}, name={self.name})"


@dataclass
class VReg:
    address: int
    dtype: DType = None
    default: int | float = 0
    description: str = ""


@dataclass
class VRegMap(YAMLWizard):
    interfaces: dict[str, dict[str, VReg]]


class Avril:
    mode: AvrilMode
    pid: int
    port: str
    device: serial.Serial
    chunk_size: int = 48

    def __init__(
        self,
        mode: AvrilMode = AvrilMode.Vmmi,
        pid: int = BRIDGE_PID,
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

    def __str__(self):
        return f"Avril(mode={self.mode}, pid={self.pid}, port={self.port})"

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
        cmd = AvrilMessage(
            AvrilCommand.Write, self.mode, msg_id, len(data), address, data
        )
        self._serial_send(cmd.encode(), chunk=chunk)
        response = self._serial_read(cobs.max_encoded_length(16) + 1)
        ack = AvrilAck.decode(response)
        return ack

    def write_reg(
        self,
        address: int,
        *data: int | float,
        dtype: DType,
        msg_id: int = None,
    ):
        """Write a register(s) of the given dtype."""
        single = len(data) == 1
        ack: list[AvrilAck] = [
            self.write(
                address + i * dtype.size,
                struct.pack(f"<{dtype.name}", d),
                msg_id,
                chunk=False,
            )
            for i, d in enumerate(data)
        ]
        if single:
            return ack[0]
        return (*ack,)

    def read(
        self, address: int, size: int, msg_id: int = None, chunk: bool = True
    ) -> AvrilAck:
        """Read data. Data is contained in the ack packet."""
        cmd = AvrilMessage(AvrilCommand.Read, self.mode, msg_id, size, address)
        self._serial_send(cmd.encode())

        expected_length = cobs.max_encoded_length(16 + size) + 1
        response = self._serial_read(expected_length, chunk)
        ack = AvrilAck.decode(response)
        return ack

    def read_reg(self, address: int, dtype: DType, n: int = 1, msg_id: int = None):
        """Read a single register with the given dtype."""
        single = n == 1
        ack: list[AvrilAck] = []
        for i in range(n):
            ack.append(
                self.read(address + i * dtype.size, dtype.size, msg_id, chunk=False)
            )
            ack[-1].data = (
                struct.unpack(f"<{dtype.name}", ack[-1].data)[0]
                if ack[-1].error == ErrorCode.NoError
                else b""
            )
        if single:
            return ack[0]
        return (*ack,)

    def read_meta(self, id: int, mode: AvrilMode = None) -> VMetaEntry:
        if mode is None:
            mode = AvrilMode.VMeta
        addr = id * 28
        cmd = AvrilMessage(AvrilCommand.Read, mode, None, 28, addr)
        self._serial_send(cmd.encode())

        expected_length = cobs.max_encoded_length(16 + 28) + 1
        response = self._serial_read(expected_length)

        ack = AvrilAck.decode(response)
        return VMetaEntry.unpack(ack.data)

    def read_all_meta(self, mode: AvrilMode = None) -> dict[str, VMetaEntry]:
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
        return VIFace(self, interface, registers)


class VIFace:
    av: Avril
    interface: str
    meta: VMetaEntry
    dtype: DType
    registers: dict[str, VReg]

    def __init__(self, av: Avril, interface: str, registers: dict[str, VReg] = {}):
        self.av = av
        self.interface = interface
        self.meta = self.av.read_all_meta()[interface]
        self.dtype = self.meta.dtype
        self.load_registers(registers)

    def __str__(self):
        return f"VIFace(interface={self.interface}, meta={self.meta}, dtype={self.dtype.name})"

    def load_registers(self, registers: dict[str, VReg]):
        self.registers = registers
        for rname, r in self.registers.items():
            if r.dtype is None:
                r.dtype = self.meta.dtype

    def load_register_file(self, regmap: Path):
        """"""
        iface_cfg = VRegMap.from_yaml_file(regmap)
        self.load_registers(iface_cfg.interfaces[self.interface])

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

    def write_reg(self, _address: int | str, *data: int | float, dtype: DType = None):
        """"""
        if dtype is None:
            dtype = self.dtype

        address = self._get_address(_address)
        return self.av.write_reg(address, *data, dtype=dtype)

    def read_reg(self, _address: int | str, n: int = 1, dtype: DType = None):
        if dtype is None:
            dtype = self.dtype
        address = self._get_address(_address)
        return self.av.read_reg(address, dtype, n=n)

    def __getitem__(self, address: int | str) -> int | float:
        ack = self.read_reg(address)
        if ack.error != ErrorCode.NoError:
            raise Exception(f"Read Ack Error: {ack.error.name}")
        return ack.data

    def __setitem__(
        self, address: int | str, data: int | float | Sequence[int | float]
    ):
        if isinstance(data, Sequence) or isinstance(data, Iterable):
            ack = self.write_reg(address, *data)
            for a in ack:
                if a.error != ErrorCode.NoError:
                    raise Exception(f"Read Ack Error: {ack.error.name}")
        else:
            ack = self.write_reg(address, data)
            if ack.error != ErrorCode.NoError:
                raise Exception(f"Read Ack Error: {ack.error.name}")

    @property
    def size(self) -> int:
        return self.meta.size

    @property
    def base_address(self) -> int:
        return self.meta.base_address

    def __iter__(self):
        return iter(range(0, self.size, self.dtype.size))
