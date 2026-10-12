"""
GHDL interface: a synthesizable VHDL entity as SystemVerilog, for the Verilator flow.

transpile() turns an entity into one SystemVerilog file that the rest of dspsim treats like any other
source: GHDL synthesizes the entity to a Verilog netlist (`ghdl synth --out=verilog`), and a generated
wrapper module, named after the entity, gives the netlist its interface back. See docs/Vhdl.md.

The netlist has lost most of what the entity declares. Generics are constants in it, every port is an
unsigned vector, a record port is one port per field, and arrays and nested records are flat vectors. The
wrapper declares the generics as parameters and the ports with their types (signed vectors, unpacked arrays,
structs for records), and connects them to the ports of the netlist. What the types are comes from GHDL
itself: `--dump-rti` prints the generics and ports of the elaborated design, with every bound evaluated.
"""

import argparse
import hashlib
import json
import keyword
import os
import re
import shutil
import subprocess
import sys
from collections.abc import Mapping, Sequence
from dataclasses import dataclass, field, replace
from itertools import product
from pathlib import Path

from dspsim.templates import render_template

VHDL_SUFFIXES = (".vhd", ".vhdl")

type GenericValue = int | float | str | bool


def is_vhdl(source: Path | str) -> bool:
    """Whether a source file is VHDL, by its suffix."""
    return Path(source).suffix.lower() in VHDL_SUFFIXES


class GhdlError(Exception):
    """GHDL failed, or printed something that dspsim does not understand."""


class GhdlNotFoundError(GhdlError):
    """There is no GHDL to run."""


def ghdl_bin() -> Path:
    """The GHDL executable: the one that DSPSIM_GHDL names, or `ghdl` on the path."""
    if configured := os.environ.get("DSPSIM_GHDL"):
        path = Path(configured)
        if not path.is_file():
            raise GhdlNotFoundError(f"DSPSIM_GHDL is not a file: {path}")
        return path
    if found := shutil.which("ghdl"):
        return Path(found)
    raise GhdlNotFoundError(
        "VHDL sources need GHDL, which was not found: install it (https://github.com/ghdl/ghdl) "
        "or set DSPSIM_GHDL to its executable"
    )


def ghdl_available() -> bool:
    """Whether there is a GHDL to run."""
    try:
        ghdl_bin()
    except GhdlNotFoundError:
        return False
    return True


def ghdl_version() -> str:
    """The first line of `ghdl --version`: "GHDL 6.0.0 (...) [...]"."""
    return _ghdl(["--version"]).splitlines()[0].strip()


# The oldest GHDL that dspsim works with. GHDL 2 writes Verilog too, but its synthesis fails on VHDL that
# the tests use; GHDL 1 does not write Verilog.
MINIMUM_VERSION = (3, 0)


def check_ghdl_version() -> None:
    """Raise GhdlError if GHDL is older than dspsim supports. A version that cannot be read passes."""
    version = ghdl_version()
    match = re.match(r"GHDL (\d+)\.(\d+)", version)
    if match and (int(match[1]), int(match[2])) < MINIMUM_VERSION:
        raise GhdlError(
            f"dspsim needs GHDL {'.'.join(map(str, MINIMUM_VERSION))} or newer, and this is '{version}'"
        )


def _ghdl(
    args: Sequence[str | Path],
    cwd: Path | None = None,
    check: bool = True,
    messages: bool = True,
) -> str:
    """
    Run GHDL and return what it printed: with its messages (standard error), or the standard output alone.
    Raises GhdlError if it fails.
    """
    command = [str(ghdl_bin()), *(str(a) for a in args)]
    result = subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    output = result.stdout + result.stderr
    if check and result.returncode:
        raise GhdlError(
            f"ghdl {' '.join(str(a) for a in args[:2])} failed:\n{output.strip()}"
        )
    return output if messages else result.stdout


@dataclass
class VhdlOptions:
    """How a VHDL entity is read."""

    # The other VHDL files of the design: packages and entities that the entity uses. Any order.
    sources: list[Path] = field(default_factory=list)
    # The VHDL standard: "87", "93", "02", "08", "19".
    std: str = "08"
    # The entity. By default, the one that is named like the source file.
    entity: str | None = None
    # Extra arguments for GHDL when it analyzes, elaborates and synthesizes: "-fsynopsys", "-P<dir>", ...
    ghdl_args: list[str] = field(default_factory=list)


# ---------------------------------------------------------------------------------------------------------
# Types
# ---------------------------------------------------------------------------------------------------------


@dataclass(frozen=True)
class VhdlRange:
    """An index range of an array: `left to right` or `left downto right`."""

    left: int
    right: int
    ascending: bool

    @property
    def length(self) -> int:
        return max(
            0,
            (self.right - self.left if self.ascending else self.left - self.right) + 1,
        )

    @property
    def low(self) -> int:
        return self.left if self.ascending else self.right

    @property
    def indices(self) -> list[int]:
        """The indices from left to right."""
        step = 1 if self.ascending else -1
        return [self.left + step * i for i in range(self.length)]


@dataclass(frozen=True)
class VhdlBits:
    """A vector of bits: a logic value, a vector of them, an integer or an enumeration."""

    width: int
    signed: bool = False
    # One std_ulogic, bit or boolean. An array of these is a vector.
    is_bit: bool = False


@dataclass(frozen=True)
class VhdlRecord:
    name: str
    # The elements, by name, in the order they are declared.
    fields: tuple[tuple[str, VhdlType], ...]

    @property
    def width(self) -> int:
        return sum(t.width for _, t in self.fields)


@dataclass(frozen=True)
class VhdlArray:
    """An array of anything but bits: an unpacked array in SystemVerilog."""

    element: VhdlType
    ranges: tuple[VhdlRange, ...]

    @property
    def width(self) -> int:
        length = 1
        for r in self.ranges:
            length *= r.length
        return length * self.element.width


type VhdlType = VhdlBits | VhdlRecord | VhdlArray


@dataclass
class VhdlGeneric:
    name: str
    # "int", "bool", "real", "string", or None for a type that dspsim does not pass on.
    kind: str | None
    value: GenericValue | None


@dataclass
class VhdlPort:
    name: str
    # "input" or "output".
    direction: str
    type: VhdlType


@dataclass
class VhdlEntity:
    """An elaborated entity: its generics with their values, and its ports with their types."""

    name: str
    generics: list[VhdlGeneric]
    ports: list[VhdlPort]


# The types that are one bit in a netlist, whatever the number of their values.
_BIT_TYPES = ("std_ulogic", "bit", "boolean")
# The vector types that are numbers in two's complement (ieee.numeric_std, ieee.numeric_bit).
_SIGNED_TYPES = ("unresolved_signed", "signed")

_RANGE = re.compile(r"(-?\d+) (to|downto) (-?\d+)")
_INTEGER_RANGE = re.compile(rf"(?:\w+ range )?{_RANGE.pattern}")
_NAMED_TYPE = re.compile(r"(\w+)((?: \([^()]*\))*)(?: of (.*))?")
_TYPE_DECLARATION = re.compile(
    r" ghdl_rtik_(?:sub)?type_\w+(?:, D=\d+)?: (\w+) is (.*)"
)
_RECORD_ELEMENT = re.compile(r"  ghdl_rtik_element: (\w+): (.*)")
_ENTITY = re.compile(r" ghdl_rtik_entity, D=\d+, sloc=(\d+):(\d+): (\w+)")
_INTERFACE = re.compile(
    r"  ghdl_rtik_(generic|port), D=\d+, sloc=(\d+):(\d+); (\w+): (.*?) := (.*)"
)
_TREE_PORT = re.compile(r"[+`]-(\w+) \[port (\w+)\]")


def _integer_width(low: int, high: int) -> tuple[int, bool]:
    """The bits of an integer of a range, and whether it is signed, as GHDL synthesizes it."""
    if low >= 0:
        return max(1, high.bit_length()), False
    return max((-low - 1).bit_length(), max(high, 0).bit_length()) + 1, True


def _enumeration_size(text: str) -> int:
    """The number of literals of an enumeration that is printed as "(a, b, 'c', ...)"."""
    count, i = 0, 1
    while i < len(text) - 1:
        if text[i] == "'":
            # A character literal, which can be a comma or a parenthesis.
            i += 3
        else:
            while i < len(text) - 1 and text[i] != ",":
                i += 1
        count += 1
        i += 2
    return count


class _Types:
    """The types of a design, by name, as GHDL prints them."""

    def __init__(self) -> None:
        # The text after "is" of every declaration of a name, and the elements of a record.
        self.declarations: dict[str, list[str]] = {}
        self.records: dict[str, list[tuple[str, str]]] = {}

    def declare(self, name: str, text: str) -> None:
        declarations = self.declarations.setdefault(name, [])
        if text not in declarations:
            declarations.append(text)

    def _declaration(self, name: str) -> str:
        declarations = self.declarations.get(name)
        if not declarations:
            raise GhdlError(f"unknown VHDL type '{name}'")
        if len(declarations) > 1:
            raise GhdlError(
                f"VHDL type '{name}' is declared more than once in the design, "
                "and dspsim cannot tell which declaration a port uses"
            )
        return declarations[0]

    def is_integer(self, text: str) -> bool:
        """Whether a type is `integer` or a subtype of it."""
        name = text.split(" ")[0]
        if name == "integer":
            return True
        if name not in self.declarations or name in self.records:
            return False
        declaration = self._declaration(name)
        return declaration != text and bool(_INTEGER_RANGE.fullmatch(declaration))

    def resolve(
        self, text: str, constraints: list[list[VhdlRange]] | None = None
    ) -> VhdlType:
        """
        The type that GHDL prints as `text`: a name, with index ranges if it is an array ("bytes_t (1 to 3)")
        or a range if it is an integer ("integer range 0 to 15"), or the definition of a type. `constraints`
        are index ranges that apply to it from outside: those of an element whose type is unconstrained.
        """
        text = text.strip()
        constraints = list(constraints or [])

        if m := _INTEGER_RANGE.fullmatch(text):
            low, high = sorted((int(m[1]), int(m[3])))
            width, signed = _integer_width(low, high)
            return VhdlBits(width, signed)
        if " range " in text and not text.startswith("array ("):
            # A subtype of an enumeration: "std_ulogic range 'U' to '-'".
            return self.resolve(text.split(" ")[0], constraints)
        if text.startswith("("):
            return VhdlBits(max(1, (_enumeration_size(text) - 1).bit_length()))
        if text.startswith("array ("):
            # An unconstrained array: "array (natural range <>, natural range <>) of element".
            indices, _, element = text.removeprefix("array (").partition(") of ")
            if not constraints or len(constraints[0]) != indices.count("range"):
                raise GhdlError(f"unconstrained VHDL array '{text}'")
            return self._array(self.resolve(element, constraints[1:]), constraints[0])

        m = _NAMED_TYPE.fullmatch(text)
        if not m:
            raise GhdlError(f"unsupported VHDL type '{text}'")
        name = m[1]
        ranges = [
            [VhdlRange(int(a), int(c), b == "to") for a, b, c in _RANGE.findall(group)]
            for group in re.findall(r"\(([^()]*)\)", m[2])
        ]
        if m[3] is not None:
            # A constrained array, with its element: "nibble_t (3 downto 0) of std_logic".
            element = self.resolve(m[3], ranges[1:] + constraints)
            resolved: VhdlType = self._array(element, ranges[0])
        elif name in _BIT_TYPES:
            return VhdlBits(1, is_bit=True)
        elif name in self.records:
            fields = tuple((f, self.resolve(t)) for f, t in self.records[name])
            return VhdlRecord(name, fields)
        else:
            resolved = self.resolve(self._declaration(name), ranges + constraints)
        if isinstance(resolved, VhdlBits) and name in _SIGNED_TYPES:
            resolved = replace(resolved, signed=True)
        return resolved

    @staticmethod
    def _array(element: VhdlType, ranges: list[VhdlRange]) -> VhdlType:
        if len(ranges) == 1 and isinstance(element, VhdlBits) and element.is_bit:
            return VhdlBits(ranges[0].length)
        return VhdlArray(element, tuple(ranges))


def _declared_name(lines: list[str] | None, line: int, column: int, name: str) -> str:
    """
    The identifier at a place in the source, if it is `name` in another case. GHDL prints identifiers in
    lower case; the wrapper has the generics and ports as they are written.
    """
    if lines is None or not 0 < line <= len(lines):
        return name
    m = re.match(r"\w+", lines[line - 1][column - 1 :])
    return m[0] if m and m[0].lower() == name else name


def _generic(name: str, type_text: str, value: str, types: _Types) -> VhdlGeneric:
    type_name = type_text.split(" ")[0]
    try:
        if type_name == "boolean":
            return VhdlGeneric(name, "bool", value == "true")
        if type_name == "string":
            return VhdlGeneric(
                name, "string", value.removeprefix('"').removesuffix('"')
            )
        if type_name == "real":
            return VhdlGeneric(name, "real", float(value))
        if types.is_integer(type_text):
            return VhdlGeneric(name, "int", int(value))
    except ValueError:
        pass
    return VhdlGeneric(name, None, None)


def parse_rti(output: str) -> VhdlEntity:
    """
    Read the top entity from what `ghdl -r <entity> --dump-rti --disp-tree=port` prints: the types of the
    packages, then the generics and ports of the entity, each with its elaborated type.
    """
    types = _Types()
    directions: dict[str, str] = {}
    entity_name: str | None = None
    source_lines: list[str] | None = None
    interface: list[tuple[str, str, str, str]] = []
    record: str | None = None
    in_entity = False

    for line in output.splitlines():
        if m := _TREE_PORT.fullmatch(line):
            directions.setdefault(m[1], m[2])
        elif entity_name is None and (m := _TYPE_DECLARATION.fullmatch(line)):
            record = m[1] if m[2] == "record" else None
            if record:
                types.records[record] = []
            else:
                types.declare(m[1], m[2])
        elif entity_name is None and record and (m := _RECORD_ELEMENT.fullmatch(line)):
            types.records[record].append((m[1], m[2]))
        elif entity_name is None and (m := _ENTITY.fullmatch(line)):
            entity_name, in_entity = m[3], True
            entity_place = (int(m[1]), int(m[2]))
        elif in_entity and line.startswith("  filename: "):
            try:
                source_lines = (
                    Path(line.removeprefix("  filename: "))
                    .read_text(errors="replace")
                    .splitlines()
                )
            except OSError:
                source_lines = None
            entity_name = _declared_name(source_lines, *entity_place, entity_name)
        elif in_entity and (m := _INTERFACE.fullmatch(line)):
            name = _declared_name(source_lines, int(m[2]), int(m[3]), m[4])
            interface.append((m[1], name, m[5], m[6]))
        elif in_entity and not line.startswith("  "):
            in_entity = False

    if entity_name is None:
        raise GhdlError(f"GHDL did not describe the design:\n{output.strip()[-2000:]}")

    generics, ports = [], []
    for kind, name, type_text, value in interface:
        if kind == "generic":
            generics.append(_generic(name, type_text, value, types))
            continue
        mode = directions.get(name.lower())
        if mode not in ("in", "out", "buffer"):
            raise GhdlError(
                f"port '{name}' of '{entity_name}' has the mode '{mode}': only in, out and buffer are supported"
            )
        try:
            port_type = types.resolve(type_text)
        except GhdlError as error:
            raise GhdlError(f"port '{name}' of '{entity_name}': {error}") from None
        ports.append(VhdlPort(name, "input" if mode == "in" else "output", port_type))
    return VhdlEntity(entity_name, generics, ports)


# ---------------------------------------------------------------------------------------------------------
# The wrapper
# ---------------------------------------------------------------------------------------------------------

# Names that a port, a generic or a record cannot have in the generated SystemVerilog, C++ and Python. They
# get an underscore at the end, which no VHDL identifier has.
_RESERVED = frozenset(
    keyword.kwlist
    + [
        "accept_on",
        "alias",
        "always",
        "always_comb",
        "always_ff",
        "always_latch",
        "and",
        "assert",
        "assign",
        "assume",
        "automatic",
        "before",
        "begin",
        "bind",
        "bins",
        "binsof",
        "bit",
        "break",
        "buf",
        "bufif0",
        "bufif1",
        "byte",
        "case",
        "casex",
        "casez",
        "cell",
        "chandle",
        "checker",
        "class",
        "clocking",
        "cmos",
        "config",
        "const",
        "constraint",
        "context",
        "continue",
        "cover",
        "covergroup",
        "coverpoint",
        "cross",
        "deassign",
        "default",
        "defparam",
        "design",
        "disable",
        "dist",
        "do",
        "edge",
        "else",
        "end",
        "endcase",
        "endchecker",
        "endclass",
        "endclocking",
        "endconfig",
        "endfunction",
        "endgenerate",
        "endgroup",
        "endinterface",
        "endmodule",
        "endpackage",
        "endprimitive",
        "endprogram",
        "endproperty",
        "endspecify",
        "endsequence",
        "endtable",
        "endtask",
        "enum",
        "event",
        "eventually",
        "expect",
        "export",
        "extends",
        "extern",
        "final",
        "first_match",
        "for",
        "force",
        "foreach",
        "forever",
        "fork",
        "forkjoin",
        "function",
        "generate",
        "genvar",
        "global",
        "highz0",
        "highz1",
        "if",
        "iff",
        "ifnone",
        "ignore_bins",
        "illegal_bins",
        "implements",
        "implies",
        "import",
        "incdir",
        "include",
        "initial",
        "inout",
        "input",
        "inside",
        "instance",
        "int",
        "integer",
        "interconnect",
        "interface",
        "intersect",
        "join",
        "join_any",
        "join_none",
        "large",
        "let",
        "liblist",
        "library",
        "local",
        "localparam",
        "logic",
        "longint",
        "macromodule",
        "matches",
        "medium",
        "modport",
        "module",
        "nand",
        "negedge",
        "nettype",
        "new",
        "nexttime",
        "nmos",
        "nor",
        "noshowcancelled",
        "not",
        "notif0",
        "notif1",
        "null",
        "or",
        "output",
        "package",
        "packed",
        "parameter",
        "pmos",
        "posedge",
        "primitive",
        "priority",
        "program",
        "property",
        "protected",
        "pull0",
        "pull1",
        "pulldown",
        "pullup",
        "pulsestyle_ondetect",
        "pulsestyle_onevent",
        "pure",
        "rand",
        "randc",
        "randcase",
        "randsequence",
        "rcmos",
        "real",
        "realtime",
        "ref",
        "reg",
        "reject_on",
        "release",
        "repeat",
        "restrict",
        "return",
        "rnmos",
        "rpmos",
        "rtran",
        "rtranif0",
        "rtranif1",
        "s_always",
        "s_eventually",
        "s_nexttime",
        "s_until",
        "s_until_with",
        "scalared",
        "sequence",
        "shortint",
        "shortreal",
        "showcancelled",
        "signed",
        "small",
        "soft",
        "solve",
        "specify",
        "specparam",
        "static",
        "string",
        "strong",
        "strong0",
        "strong1",
        "struct",
        "super",
        "supply0",
        "supply1",
        "sync_accept_on",
        "sync_reject_on",
        "table",
        "tagged",
        "task",
        "this",
        "throughout",
        "time",
        "timeprecision",
        "timeunit",
        "tran",
        "tranif0",
        "tranif1",
        "tri",
        "tri0",
        "tri1",
        "triand",
        "trior",
        "trireg",
        "type",
        "typedef",
        "union",
        "unique",
        "unique0",
        "unsigned",
        "until",
        "until_with",
        "untyped",
        "use",
        "uwire",
        "var",
        "vectored",
        "virtual",
        "void",
        "wait",
        "wait_order",
        "wand",
        "weak",
        "weak0",
        "weak1",
        "while",
        "wildcard",
        "wire",
        "with",
        "within",
        "wor",
        "xnor",
        "xor",
        "alignas",
        "alignof",
        "and_eq",
        "asm",
        "auto",
        "bitand",
        "bitor",
        "bool",
        "catch",
        "char",
        "char8_t",
        "char16_t",
        "char32_t",
        "co_await",
        "co_return",
        "co_yield",
        "compl",
        "concept",
        "consteval",
        "constexpr",
        "constinit",
        "const_cast",
        "decltype",
        "delete",
        "double",
        "dynamic_cast",
        "explicit",
        "float",
        "friend",
        "goto",
        "inline",
        "long",
        "mutable",
        "namespace",
        "noexcept",
        "not_eq",
        "nullptr",
        "operator",
        "or_eq",
        "private",
        "public",
        "register",
        "reinterpret_cast",
        "requires",
        "short",
        "sizeof",
        "static_assert",
        "static_cast",
        "switch",
        "template",
        "thread_local",
        "throw",
        "try",
        "typeid",
        "typename",
        "using",
        "volatile",
        "wchar_t",
        "xor_eq",
    ]
)


def _sv_name(name: str) -> str:
    return f"{name}_" if name in _RESERVED or name.lower() in _RESERVED else name


def _sv_vector(bits: VhdlBits) -> str:
    """The SystemVerilog type of a vector of bits."""
    if bits.is_bit:
        return "logic"
    return f"logic {'signed ' if bits.signed else ''}[{bits.width - 1}:0]"


def _sv_declaration(vhdl_type: VhdlType, name: str) -> str:
    """The declaration of a port, or of a member of a struct, of a type: "logic signed [7:0] taps [4]"."""
    dimensions = ""
    while isinstance(vhdl_type, VhdlArray):
        dimensions += "".join(f"[{r.length}]" for r in vhdl_type.ranges)
        vhdl_type = vhdl_type.element
    element = (
        _sv_name(vhdl_type.name)
        if isinstance(vhdl_type, VhdlRecord)
        else _sv_vector(vhdl_type)
    )
    return f"{element} {name}{' ' + dimensions if dimensions else ''}"


def _records(vhdl_type: VhdlType, found: dict[str, VhdlRecord]) -> None:
    """Collect the records of a type, each after the records that it has as elements."""
    if isinstance(vhdl_type, VhdlArray):
        _records(vhdl_type.element, found)
    elif isinstance(vhdl_type, VhdlRecord):
        for _, field_type in vhdl_type.fields:
            _records(field_type, found)
        if found.setdefault(vhdl_type.name, vhdl_type) != vhdl_type:
            raise GhdlError(
                f"VHDL record '{vhdl_type.name}' is used with different element types"
            )


def _leaves(expression: str, vhdl_type: VhdlType) -> list[str]:
    """
    The vectors that a value consists of, as SystemVerilog expressions, in the order GHDL packs them into one
    vector, most significant first. The elements of a record are packed from the least significant bit in
    the order they are declared; the elements of an array from the most significant bit, left to right. An
    element of an array has the index of its position from the lowest index of the VHDL range.
    """
    if isinstance(vhdl_type, VhdlBits):
        return [expression]
    if isinstance(vhdl_type, VhdlRecord):
        return [
            leaf
            for name, field_type in reversed(vhdl_type.fields)
            for leaf in _leaves(f"{expression}.{_sv_name(name)}", field_type)
        ]
    return [
        leaf
        for indices in product(*(r.indices for r in vhdl_type.ranges))
        for leaf in _leaves(
            expression
            + "".join(f"[{i - r.low}]" for i, r in zip(indices, vhdl_type.ranges)),
            vhdl_type.element,
        )
    ]


_MODULE = re.compile(r"^module (\S+)\s*$")
_NETLIST_PORT = re.compile(
    r"\s*\(?(input|output)\s+(?:\[(\d+):0\]\s+)?(\\\S+ |\w+)\s*(?:,|\);)\s*"
)
_ANONYMOUS_NET = re.compile(
    r"\s*(?:wire|reg|localparam)\s+(?:\[[^\]]+\]\s+)?n\d+\b[^;]*;\s*"
)


def _prepare_netlist(
    netlist: str, entity: str, module: str
) -> tuple[str, dict[str, tuple[str, int]]]:
    """
    Rename the module of the entity in the netlist of GHDL, which is the last one, to `module`, and keep the
    nets that GHDL made up for expressions (n123) out of the traces of Verilator. Returns the netlist and
    the ports of the module: the name in the netlist and the width, by the name in lower case, which is
    "port" or "port[element]" for an element of a record port.
    """
    lines = netlist.splitlines()
    modules = [i for i, line in enumerate(lines) if _MODULE.match(line)]
    if not modules or _MODULE.match(lines[modules[-1]])[1].lower() != entity.lower():  # type: ignore[index]
        raise GhdlError(f"GHDL did not synthesize the entity '{entity}'")
    if any(_MODULE.match(lines[i])[1] == module for i in modules):  # type: ignore[index]
        raise GhdlError(
            f"the design has a unit called '{module}', which is the name dspsim gives the netlist of '{entity}'"
        )
    top = modules[-1]
    lines[top] = f"module {module}"

    ports: dict[str, tuple[str, int]] = {}
    for line in lines[top + 1 :]:
        m = _NETLIST_PORT.fullmatch(line)
        if not m:
            break
        ports[m[3].strip().removeprefix("\\").lower()] = (m[3], int(m[2] or 0) + 1)

    result: list[str] = []
    hidden = False
    for line in lines:
        anonymous = bool(_ANONYMOUS_NET.fullmatch(line))
        if anonymous != hidden:
            result.append(
                "  /* verilator tracing_off */"
                if anonymous
                else "  /* verilator tracing_on */"
            )
            hidden = anonymous
        result.append(line)
    return "\n".join(result), ports


def _parameter(generic: VhdlGeneric) -> str | None:
    """The declaration of the parameter of the wrapper for a generic, or None if it has none."""
    name = _sv_name(generic.name)
    match generic.kind:
        case "int":
            return f"parameter int {name} = {generic.value}"
        case "bool":
            return f"parameter bit {name} = 1'b{int(bool(generic.value))}"
        case "real":
            return f"parameter real {name} = {generic.value!r}"
        case "string":
            text = str(generic.value).replace("\\", "\\\\").replace('"', '\\"')
            return f'parameter string {name} = "{text}"'
    return None


def wrapper(entity: VhdlEntity, netlist: str, comment: str = "") -> str:
    """
    The SystemVerilog source of an entity: a module with its name, generics and ports, around the netlist
    that GHDL synthesized for it with the same values of the generics.
    """
    module = f"{entity.name}_ghdl"
    netlist, netlist_ports = _prepare_netlist(netlist, entity.name, module)

    records: dict[str, VhdlRecord] = {}
    for port in entity.ports:
        _records(port.type, records)
    typedefs = [
        (
            _sv_name(record.name),
            [_sv_declaration(t, _sv_name(f)) for f, t in record.fields],
        )
        for record in records.values()
    ]

    # Each port of the netlist is one vector: connect it to the vectors that it is made of.
    wires: list[tuple[int, str]] = []
    assigns: list[str] = []
    connections: list[tuple[str, str]] = []

    def connect(
        netlist_names: Sequence[str],
        expression: str,
        vhdl_type: VhdlType,
        direction: str,
    ) -> None:
        netlist_name = next(
            (n for n in netlist_names if n in netlist_ports), netlist_names[0]
        )
        if netlist_name not in netlist_ports:
            raise GhdlError(
                f"the netlist of '{entity.name}' has no port '{netlist_name}'"
            )
        name, width = netlist_ports.pop(netlist_name)
        if width != vhdl_type.width:
            raise GhdlError(
                f"port '{netlist_name}' of '{entity.name}' has {width} bits in the netlist of GHDL, "
                f"and {vhdl_type.width} bits as dspsim reads its type"
            )
        # The name is written as an escaped identifier, which a keyword of SystemVerilog can be too.
        name = name if name.startswith("\\") else f"\\{name} "
        leaves = _leaves(expression, vhdl_type)
        if len(leaves) == 1:
            connections.append((name, leaves[0]))
            return
        # A name that ends with an underscore is not a VHDL identifier: it is not the name of a port.
        wire = f"flat_{len(wires)}_"
        wires.append((width, wire))
        packed = "{" + ", ".join(leaves) + "}"
        assigns.append(
            f"assign {wire} = {packed};"
            if direction == "input"
            else f"assign {packed} = {wire};"
        )
        connections.append((name, wire))

    for port in entity.ports:
        name = _sv_name(port.name)
        lower = port.name.lower()
        if isinstance(port.type, VhdlRecord):
            # GHDL gives a record port a port for each of its elements: "port[element]" (GHDL 6),
            # "port_port[element]" (GHDL 5.1) or "port_element" (before).
            for element, element_type in port.type.fields:
                connect(
                    (
                        f"{lower}[{element}]",
                        f"{lower}_{lower}[{element}]",
                        f"{lower}_{element}",
                    ),
                    f"{name}.{_sv_name(element)}",
                    element_type,
                    port.direction,
                )
        else:
            connect((lower,), name, port.type, port.direction)
    if netlist_ports:
        raise GhdlError(
            f"the netlist of '{entity.name}' has ports that the entity does not: {', '.join(netlist_ports)}"
        )

    return render_template(
        "vhdl_wrapper.sv.jinja",
        entity=entity.name,
        module=module,
        comment=comment,
        typedefs=typedefs,
        parameters=[p for p in map(_parameter, entity.generics) if p],
        ports=[
            f"{port.direction:6} {_sv_declaration(port.type, _sv_name(port.name))}"
            for port in entity.ports
        ],
        wires=wires,
        assigns=assigns,
        connections=connections,
        netlist=netlist,
    )


# ---------------------------------------------------------------------------------------------------------
# Running GHDL
# ---------------------------------------------------------------------------------------------------------


def _generic_arguments(
    generics: Mapping[str, GenericValue], entity: VhdlEntity
) -> list[str]:
    """The -g arguments that give the generics of an entity the values."""
    declared = {_sv_name(g.name).lower(): g for g in entity.generics}
    arguments = []
    for name, value in generics.items():
        generic = declared.get(name.lower())
        if generic is None:
            raise GhdlError(f"'{entity.name}' has no generic '{name}'")
        if generic.kind is not None and value == generic.value:
            # The default: nothing to set, and GHDL could not set it if it is a real.
            continue
        match generic.kind:
            case "int":
                text = str(int(value))
            case "bool":
                text = "true" if value else "false"
            case "string":
                text = str(value)
            case "real":
                raise GhdlError(
                    f"generic '{name}' of '{entity.name}' is a real, which GHDL cannot set"
                )
            case _:
                raise GhdlError(
                    f"generic '{name}' of '{entity.name}' has a type that dspsim cannot set"
                )
        arguments.append(f"-g{generic.name}={text}")
    return arguments


def _digest(
    source: Path,
    generics: Mapping[str, GenericValue],
    options: VhdlOptions,
    files: list[Path],
) -> str:
    """What the SystemVerilog of an entity depends on: the sources, the options, GHDL and this generator."""
    templates = Path(__file__).parent / "templates"
    description = {
        "ghdl": ghdl_version(),
        "source": str(source),
        "entity": options.entity,
        "std": options.std,
        "ghdl_args": options.ghdl_args,
        "generics": sorted((k.lower(), repr(v)) for k, v in generics.items()),
        "files": [(str(f), hashlib.sha256(f.read_bytes()).hexdigest()) for f in files],
        "generator": [
            hashlib.sha256(f.read_bytes()).hexdigest()
            for f in (Path(__file__), templates / "vhdl_wrapper.sv.jinja")
        ],
    }
    return hashlib.sha256(json.dumps(description).encode()).hexdigest()[:12]


def transpile(
    source: Path,
    generics: Mapping[str, GenericValue] | None,
    options: VhdlOptions | None,
    output_dir: Path,
) -> Path:
    """
    Write the SystemVerilog of the VHDL entity in `source`, with the given values of its generics, and return
    its path: <output_dir>/<entity>_<digest>/<Entity>.sv. An entity that was transpiled before from the same
    sources, with the same generics and options, is not transpiled again.
    """
    source = Path(source).resolve()
    options = options or VhdlOptions()
    generics = dict(generics or {})
    files = sorted({source, *(Path(f).resolve() for f in options.sources)})
    for file in files:
        if not file.is_file():
            raise FileNotFoundError(f"VHDL source not found: {file}")
    entity_name = options.entity or source.stem

    directory = (
        Path(output_dir).resolve()
        / f"{entity_name}_{_digest(source, generics, options, files)}"
    )
    if existing := sorted(directory.glob("*.sv")):
        return existing[0]

    check_ghdl_version()
    work = directory / "work"
    work.mkdir(parents=True, exist_ok=True)
    common = [f"--std={options.std}", f"--workdir={work}", *options.ghdl_args]

    # Import every file and let GHDL analyze what the entity needs, in the order it needs it.
    _ghdl(["-i", *common, *files], cwd=work)
    _ghdl(["-m", *common, entity_name], cwd=work)

    def elaborate(arguments: list[str]) -> VhdlEntity:
        # The description is printed before the simulation starts, and the simulation then stops at once.
        # GHDL has no option to print it without starting; whether the simulation fails does not matter.
        run = [
            "-r",
            *common,
            entity_name,
            *arguments,
            "--dump-rti",
            "--disp-tree=port",
            "--stop-time=0ns",
        ]
        output = _ghdl(run, cwd=work, check=False)
        if not _ENTITY.search(output):
            raise GhdlError(
                f"GHDL could not elaborate '{entity_name}':\n{output.strip()[-2000:]}"
            )
        return parse_rti(output)

    # The types of the generics say how to write their values; the ports depend on the values.
    entity = elaborate([])
    arguments = _generic_arguments(generics, entity)
    if arguments:
        entity = elaborate(arguments)
    # The netlist is what GHDL writes to its standard output: its option for a file (-o) came with GHDL 5.
    netlist = _ghdl(
        ["synth", *common, *arguments, "--out=verilog", entity_name],
        cwd=work,
        messages=False,
    )
    (work / "netlist.v").write_text(netlist)

    comment = f"{source.name}, entity {entity.name}, synthesized by {ghdl_version()}"
    text = wrapper(entity, netlist, comment)
    # Written under another name first: the file is what says that the directory is complete.
    path = directory / f"{entity.name}.sv"
    temporary = directory / f"{entity.name}.sv.tmp"
    temporary.write_text(text)
    temporary.replace(path)
    return path


def main(argv: Sequence[str] | None = None) -> None:
    """`python -m dspsim.ghdl`: transpile an entity and print the path of its SystemVerilog. For CMake."""
    parser = argparse.ArgumentParser(
        description="Write the SystemVerilog of a VHDL entity and print its path"
    )
    parser.add_argument("source", type=Path, help="the VHDL file of the entity")
    parser.add_argument(
        "sources", type=Path, nargs="*", help="the other VHDL files of the design"
    )
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--entity", help="the entity, if it is not named like the source file"
    )
    parser.add_argument("--std", default="08")
    parser.add_argument(
        "-g", "--generic", action="append", default=[], metavar="NAME=VALUE"
    )
    parser.add_argument("--ghdl-arg", action="append", default=[])
    args = parser.parse_args(argv)

    generics: dict[str, GenericValue] = {}
    for assignment in args.generic:
        name, _, value = assignment.partition("=")
        try:
            generics[name] = int(value)
        except ValueError:
            generics[name] = {"true": True, "false": False}.get(value.lower(), value)
    options = VhdlOptions(
        sources=args.sources, std=args.std, entity=args.entity, ghdl_args=args.ghdl_arg
    )
    try:
        print(transpile(args.source, generics, options, args.output_dir).as_posix())
    except (GhdlError, FileNotFoundError) as error:
        sys.exit(f"dspsim.ghdl: {error}")


if __name__ == "__main__":
    main()
