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
from dspsim.util import iterany

from typing import Sequence
import time
import argparse


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


_fmt_lookup_tbl = {
    DType.x: "x",
    DType.int8: "b",
    DType.uint8: "B",
    DType.int16: "h",
    DType.uint16: "H",
    DType.int32: "l",
    DType.uint32: "L",
    DType.int64: "q",
    DType.uint64: "Q",
    DType.float: "f",
    DType.double: "d",
    DType.str4: "4s",
    DType.str8: "8s",
    DType.str16: "16s",
    DType.str32: "32s",
    DType.str64: "64s",
}


def fmt_lookup(dtype: DType):
    return _fmt_lookup_tbl[dtype]


def unpack_dtype(b: bytes, dtype: DType):
    return struct.unpack(f"<{fmt_lookup(dtype)}", b)[0]


def pack_dtype(x, dtype: DType) -> bytes:
    return struct.pack(f"<{fmt_lookup(dtype)}", x)


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


VMetaEntrySize = 28
VMetaReserveSize = 4096


@dataclass
class VReg:
    address: int
    dtype: DType = None
    default: int | float = 0
    description: str = ""


@dataclass
class VRegMap(YAMLWizard):
    interfaces: dict[str, dict[str, VReg]]


def serial_read_delim(
    device: serial.Serial, delim: bytes = b"\0", timeout: float = 1.0
) -> bytes:
    buf = b""
    start_time = time.time()
    while time.time() - start_time < timeout:
        new_byte = device.read()
        buf += new_byte
        if new_byte == delim:
            break

    return buf


class Avril:
    mode: AvrilMode
    pid: int
    port: str
    device: serial.Serial

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
        self.meta_address = 0

    def __str__(self):
        return f"Avril(mode={self.mode}, pid={self.pid}, port={self.port})"

    def __enter__(self):
        time.sleep(0.2)
        self.device.port = self.port
        self.device.open()
        return self

    def __exit__(self, *args):
        self.device.flush()
        self.device.close()

    def write(self, address: int, data: bytes, msg_id: int = None) -> AvrilAck:
        cmd = AvrilMessage(
            AvrilCommand.Write, self.mode, msg_id, len(data), address, data
        )
        self.device.write(cmd.encode())
        response = serial_read_delim(self.device)
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
                pack_dtype(d, dtype),
                msg_id,
            )
            for i, d in enumerate(data)
        ]
        if single:
            return ack[0]
        return (*ack,)

    def read(self, address: int, size: int, msg_id: int = None) -> AvrilAck:
        """Read data. Data is contained in the ack packet."""
        cmd = AvrilMessage(AvrilCommand.Read, self.mode, msg_id, size, address)
        self.device.write(cmd.encode())
        response = serial_read_delim(self.device)
        ack = AvrilAck.decode(response)
        return ack

    def read_reg(self, address: int, dtype: DType, n: int = 1, msg_id: int = None):
        """Read a single register with the given dtype."""
        single = n == 1
        ack: list[AvrilAck] = []
        for i in range(n):
            ack.append(self.read(address + i * dtype.size, dtype.size, msg_id))
            ack[-1].data = (
                unpack_dtype(ack[-1].data, dtype)
                if ack[-1].error == ErrorCode.NoError
                else b""
            )
        if single:
            return ack[0]
        return (*ack,)

    def read_meta(self, id: int, mode: AvrilMode = None) -> VMetaEntry:
        if mode is None:
            mode = AvrilMode.VMeta
        addr = id * VMetaEntrySize
        cmd = AvrilMessage(AvrilCommand.Read, mode, None, VMetaEntrySize, addr)
        self.device.write(cmd.encode())
        response = serial_read_delim(self.device)

        ack = AvrilAck.decode(response)
        return VMetaEntry.unpack(ack.data)

    def read_all_meta(self, mode: AvrilMode = None) -> dict[str, VMetaEntry]:
        """"""
        all_meta = {}
        max = int(VMetaReserveSize / VMetaEntrySize)
        for i in range(max):
            try:
                entry = self.read_meta(i, mode)
                all_meta[entry.name] = entry
            except Exception:
                break
        return all_meta

    def get_interface(self, interface: str, registers: dict[str, VReg] = {}):
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
        if address >= self.size:
            raise Exception("Invalid Address for this interface.")
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
        ack = self.write_reg(address, *iterany(data))
        for a in iterany(ack):
            if a.error != ErrorCode.NoError:
                raise Exception(f"Read Ack Error: {ack.error.name}")

    @property
    def size(self) -> int:
        return self.meta.size

    @property
    def base_address(self) -> int:
        return self.meta.base_address

    def __iter__(self):
        return iter(range(0, self.size, self.dtype.size))


def dtype_lookup(s: str) -> DType:
    return DType._value2member_map_[s]


def dtype_cnv(dtype: DType, d: str) -> int | float:
    if dtype.name in ["f", "d"]:
        return float(d)
    else:
        return int(d)


@dataclass
class Args:
    command: str  # write or read
    address: int
    data: list[int | float]
    n: int = 1
    dtype: str = None
    interface: str = None
    verbose: bool = False

    @classmethod
    def parse_args(cls, cli_args: list[str] = None):
        parser = argparse.ArgumentParser()
        parser.add_argument(
            "command",
            type=str,
            choices=["write", "read"],
            help="write or read command.",
        )
        parser.add_argument("-address", type=int, help="Command address")
        parser.add_argument("-data", nargs="+", help="Write data")
        parser.add_argument(
            "-n", type=int, default=1, help="Read a number of registers."
        )
        parser.add_argument("-dtype", type=str, default=None, help="DType")
        parser.add_argument(
            "-interface",
            type=str,
            default=None,
            help="Address will be relative to this interface.",
        )
        parser.add_argument("-verbose", action="store_true", help="verbose output")
        return cls(**vars(parser.parse_args(cli_args)))


def main(cli_args: list[str] = None):
    args = Args.parse_args(cli_args)
    # print(args)

    with Avril(timeout=0.05) as av:
        if args.interface:
            iface = av.get_interface(args.interface)
            dtype = dtype_lookup(args.dtype) if args.dtype else iface.dtype
            wr_func = iface.write_reg
            rd_func = iface.read_reg
        else:
            dtype = dtype_lookup(args.dtype)
            wr_func = av.write_reg
            rd_func = av.read_reg

        # Execute command
        if args.command == "write":
            data = [dtype_cnv(dtype, d) for d in args.data]
            ack = wr_func(args.address, *data, dtype=dtype)
        elif args.command == "read":
            ack = rd_func(args.address, dtype=dtype, n=args.n)

        # Check errors
        for a in iterany(ack):
            if a.error != ErrorCode.NoError:
                raise Exception(f"Ack Error: {a}")

        if args.command == "read":
            msg = (
                "\n".join([str(a) for a in iterany(ack)])
                if args.verbose
                else ", ".join([str(a.data) for a in iterany(ack)])
            )
        elif args.command == "write":
            msg = "\n".join([str(a) for a in iterany(ack)]) if args.verbose else ""

        print(msg)
