"""VHDL models: GHDL synthesizes the entity to Verilog, and a generated SystemVerilog wrapper restores its interface."""

import importlib.util
import random
import re
import subprocess
import sys
from pathlib import Path

import pytest

import dspsim
from dspsim import framework, ghdl
from dspsim.builder import VModule, build_module, build_vmodule, vbuilder
from dspsim.framework import (
    Clock,
    Context,
    InputArrayS8,
    InputArrayU8,
    InputArrayU32,
    InputS8,
    InputS16,
    InputU8,
    OutputArrayU8,
    OutputArrayU32,
    OutputS8,
    OutputS16,
    OutputU8,
    PortStruct,
    PortStructArray,
    SignalS16,
    SignalU8,
    from_words,
    to_words,
)
from dspsim.ghdl import (
    GhdlError,
    VhdlArray,
    VhdlBits,
    VhdlEntity,
    VhdlGeneric,
    VhdlPort,
    VhdlRange,
    VhdlRecord,
    parse_rti,
)
from dspsim.project import ProjectConfig

HDL_DIR = Path(__file__).parent.parent / "hdl"
VHDL_DIR = HDL_DIR / "vhdl"
VHDL_MODEL_SOURCES = [VHDL_DIR / "vhdl_model_pkg.vhd", VHDL_DIR / "Accumulator.vhd"]

# The ports of VhdlModel but its clock and reset: an input and an output of every kind.
VHDL_MODEL_PORTS = [
    f"{name}_{direction}"
    for name in (
        "data",
        "wide",
        "count",
        "state",
        "flag",
        "vec",
        "mat",
        "bus",
        "buses",
        "frame",
    )
    for direction in ("in", "out")
] + ["acc_out"]

# The tests that run GHDL. The others check how dspsim reads what GHDL prints.
needs_ghdl = pytest.mark.skipif(
    not ghdl.ghdl_available(), reason="GHDL is not installed"
)


# What `ghdl -r <entity> --dump-rti --disp-tree=port` prints (GHDL 6.0.0), shortened.
RTI = """\
demo [entity]
+-clk [port in]
+-data [port in]
+-level [port in]
+-bus_in [port in]
+-rows [port out]
+-q [port buffer]
`-rtl [arch]
  +-u_sub [instance]
  | `-sub [entity]
  |   +-clk [port in]
  |   `-data [port inout]
DISP_RTI.Disp_All: ghdl_rtik_top
ghdl_rtik_package, D=1, sloc=1:1: standard
 filename: *std_standard*
 ghdl_rtik_type_b1: boolean is (false, true)
 ghdl_rtik_type_b1: bit is ('0', '1')
 ghdl_rtik_type_e8: character is (nul, ' ', '(', ')', ',', ''', 'a')
 ghdl_rtik_subtype_scalar, D=0: integer is -2147483648 to 2147483647
 ghdl_rtik_subtype_scalar, D=0: real is -1.79769e+308 to 1.79769e+308
 ghdl_rtik_subtype_scalar, D=0: natural is integer range 0 to 2147483647
 ghdl_rtik_subtype_scalar, D=0: positive is integer range 1 to 2147483647
 ghdl_rtik_type_array: string is array (positive range <>) of character
 ghdl_rtik_type_array: bit_vector is array (natural range <>) of bit
ghdl_rtik_package, D=1, sloc=56:9: std_logic_1164
 filename: ../../src/ieee2008/std_logic_1164.vhdl
 ghdl_rtik_type_e8: std_ulogic is ('U', 'X', '0', '1', 'Z', 'W', 'L', 'H', '-')
 ghdl_rtik_type_array: std_ulogic_vector is array (natural range <>) of std_ulogic
 ghdl_rtik_subtype_scalar, D=0: std_logic is std_ulogic range 'U' to '-'
 ghdl_rtik_subtype_unbounded_array: std_logic_vector is std_ulogic_vector
ghdl_rtik_package, D=1, sloc=60:9: numeric_std
 filename: ../../src/ieee2008/numeric_std.vhdl
 ghdl_rtik_type_array: unresolved_unsigned is array (natural range <>) of std_ulogic
 ghdl_rtik_type_array: unresolved_signed is array (natural range <>) of std_ulogic
 ghdl_rtik_subtype_unbounded_array: unsigned is unresolved_unsigned
 ghdl_rtik_subtype_unbounded_array: signed is unresolved_signed
ghdl_rtik_package, D=1, sloc=5:9: demo_pkg
 filename: demo_pkg.vhd
 ghdl_rtik_subtype_array: byte_t is std_ulogic_vector (7 downto 0)
 ghdl_rtik_subtype_array: nibble_t is nibble_t (3 downto 0) of std_logic
 ghdl_rtik_type_e8: state_t is (idle, load, run, done, fail)
 ghdl_rtik_type_array: bytes_t is array (natural range <>) of byte_t
 ghdl_rtik_type_array: words_t is array (natural range <>) of std_logic_vector
 ghdl_rtik_subtype_array: matrix_t is matrix_t (0 to 1, 2 downto 0) of unresolved_signed (7 downto 0)
 ghdl_rtik_type_record: iq_t is record
  ghdl_rtik_element: re: unresolved_signed (7 downto 0)
  ghdl_rtik_element: im: unresolved_signed (7 downto 0)
 ghdl_rtik_type_record: bus_t is record
  ghdl_rtik_element: data: unresolved_signed (15 downto 0)
  ghdl_rtik_element: valid: std_logic
  ghdl_rtik_element: state: state_t
  ghdl_rtik_element: last: boolean
  ghdl_rtik_element: iq: iq_t
  ghdl_rtik_element: taps: bytes_t (2 downto 1)
ghdl_rtik_architecture, D=1, sloc=30:14: rtl
 filename: demo.vhd
 ghdl_rtik_entity, D=1, sloc=8:8: demo
  filename: demo.vhd
  ghdl_rtik_generic, D=1, sloc=10:5; dw: positive := 12
  ghdl_rtik_generic, D=1, sloc=11:5; offset: integer := -2
  ghdl_rtik_generic, D=1, sloc=12:5; mode: string (1 to 3) := "add"
  ghdl_rtik_generic, D=1, sloc=13:5; invert: boolean := false
  ghdl_rtik_generic, D=1, sloc=14:5; scale: real := 0.5
  ghdl_rtik_generic, D=1, sloc=15:5; init: std_ulogic_vector (3 downto 0) := "1010"
  ghdl_rtik_port, D=1, sloc=18:5; clk: std_logic := 'U'
  ghdl_rtik_port, D=1, sloc=19:5; data: unresolved_signed (11 downto 0) := "UUUUUUUUUUUU"
  ghdl_rtik_port, D=1, sloc=20:5; level: integer range -5 to 5 := -5
  ghdl_rtik_port, D=1, sloc=21:5; bus_in: bus_t := (data => "UUUUUUUUUUUUUUUU", valid => 'U', state => idle)
  ghdl_rtik_port, D=1, sloc=22:5; rows: words_t (1 to 2) (5 downto 0) := ("UUUUUU", "UUUUUU")
  ghdl_rtik_port, D=1, sloc=23:5; q: std_logic := 'U'
 ghdl_rtik_subtype_array: ram_t is ram_t (0 to 15) of std_ulogic_vector (15 downto 0)
 ghdl_rtik_signal, D=1, sloc=32:10; ram: ram_t := ("0000000000000000")
 ghdl_rtik_instance, sloc=40:3: u_sub
  ghdl_rtik_architecture, D=1, sloc=28:14: rtl
   ghdl_rtik_entity, D=1, sloc=18:8: sub
    ghdl_rtik_generic, D=1, sloc=19:12; w: positive := 16
    ghdl_rtik_port, D=1, sloc=21:5; clk: std_logic := 'U'
"""


def test_parse_rti():
    """The generics and ports of the top entity, with the types that the packages declare."""
    entity = parse_rti(RTI)
    assert entity.name == "demo"
    assert [(g.name, g.kind, g.value) for g in entity.generics] == [
        ("dw", "int", 12),
        ("offset", "int", -2),
        ("mode", "string", "add"),
        ("invert", "bool", False),
        ("scale", "real", 0.5),
        # A generic of a type that dspsim does not pass on.
        ("init", None, None),
    ]

    ports = {port.name: port for port in entity.ports}
    assert list(ports) == ["clk", "data", "level", "bus_in", "rows", "q"]
    # A buffer is an output. The ports of the instances inside are not ports of the entity.
    assert [port.direction for port in entity.ports] == [
        "input",
        "input",
        "input",
        "input",
        "output",
        "output",
    ]
    assert ports["clk"].type == VhdlBits(1, is_bit=True)
    assert ports["data"].type == VhdlBits(12, signed=True)
    assert ports["level"].type == VhdlBits(4, signed=True)
    # An array of vectors, whose element type is constrained by the port.
    assert ports["rows"].type == VhdlArray(VhdlBits(6), (VhdlRange(1, 2, True),))

    bus = ports["bus_in"].type
    assert isinstance(bus, VhdlRecord)
    assert dict(bus.fields) == {
        "data": VhdlBits(16, signed=True),
        "valid": VhdlBits(1, is_bit=True),
        # An enumeration of five values, and a boolean.
        "state": VhdlBits(3),
        "last": VhdlBits(1, is_bit=True),
        "iq": VhdlRecord(
            "iq_t", (("re", VhdlBits(8, signed=True)), ("im", VhdlBits(8, signed=True)))
        ),
        "taps": VhdlArray(VhdlBits(8), (VhdlRange(2, 1, False),)),
    }
    assert bus.width == 16 + 1 + 3 + 1 + 16 + 16


def test_vhdl_types():
    types = ghdl._Types()
    for line in RTI.splitlines():
        if m := ghdl._TYPE_DECLARATION.fullmatch(line):
            types.declare(m[1], m[2])

    # Integers: the bits that hold the range.
    assert types.resolve("integer") == VhdlBits(32, signed=True)
    assert types.resolve("natural") == VhdlBits(31)
    assert types.resolve("integer range 0 to 100") == VhdlBits(7)
    assert types.resolve("integer range 1 to 9") == VhdlBits(4)
    assert types.resolve("integer range -8 to 7") == VhdlBits(4, signed=True)
    assert types.resolve("integer range -9 to 7") == VhdlBits(5, signed=True)
    assert types.is_integer("positive") and types.is_integer("integer range 1 to 9")
    assert not types.is_integer("real") and not types.is_integer("state_t")

    # Arrays of bits are vectors, signed if the type is.
    assert types.resolve("std_logic_vector (7 downto 0)") == VhdlBits(8)
    assert types.resolve("bit_vector (0 to 4)") == VhdlBits(5)
    assert types.resolve("signed (3 downto 0)") == VhdlBits(4, signed=True)
    assert types.resolve("unsigned (3 downto 0)") == VhdlBits(4)
    assert types.resolve("byte_t") == VhdlBits(8)
    assert types.resolve("nibble_t") == VhdlBits(4)
    # A vector of one bit is not a bit: an array of them is an array.
    assert types.resolve("std_logic_vector (0 downto 0)") == VhdlBits(1)
    assert types.resolve("std_logic") == VhdlBits(1, is_bit=True)

    # Enumerations, whose literals can be characters.
    assert types.resolve("state_t") == VhdlBits(3)
    assert types.resolve("character") == VhdlBits(3)

    # Arrays of anything else, with one or more dimensions.
    assert types.resolve("bytes_t (0 to 2)") == VhdlArray(
        VhdlBits(8), (VhdlRange(0, 2, True),)
    )
    matrix = types.resolve("matrix_t")
    assert matrix == VhdlArray(
        VhdlBits(8, signed=True), (VhdlRange(0, 1, True), VhdlRange(2, 0, False))
    )
    assert matrix.width == 48

    with pytest.raises(GhdlError, match="unconstrained VHDL array"):
        types.resolve("bytes_t")
    with pytest.raises(GhdlError, match="unknown VHDL type 'missing_t'"):
        types.resolve("missing_t")
    types.declare("byte_t", "std_ulogic_vector (8 downto 0)")
    with pytest.raises(GhdlError, match="'byte_t' is declared more than once"):
        types.resolve("byte_t")


def test_vhdl_leaves():
    """The order in which GHDL packs the parts of a value into one vector, most significant first."""
    iq = VhdlRecord("iq_t", (("re", VhdlBits(8)), ("im", VhdlBits(8))))
    # The elements of a record from the least significant bit, in the order they are declared.
    assert ghdl._leaves("x", iq) == ["x.im", "x.re"]
    # The elements of an array from the most significant bit, left to right. The index is the place from
    # the lowest index of the range.
    assert ghdl._leaves("x", VhdlArray(VhdlBits(8), (VhdlRange(0, 2, True),))) == [
        "x[0]",
        "x[1]",
        "x[2]",
    ]
    assert ghdl._leaves("x", VhdlArray(VhdlBits(8), (VhdlRange(3, 1, False),))) == [
        "x[2]",
        "x[1]",
        "x[0]",
    ]
    assert ghdl._leaves("x", VhdlArray(VhdlBits(8), (VhdlRange(5, 6, True),))) == [
        "x[0]",
        "x[1]",
    ]
    matrix = VhdlArray(VhdlBits(8), (VhdlRange(0, 1, True), VhdlRange(1, 0, False)))
    assert ghdl._leaves("m", matrix) == ["m[0][1]", "m[0][0]", "m[1][1]", "m[1][0]"]
    # An array of records, and a name that is a keyword.
    samples = VhdlArray(iq, (VhdlRange(1, 0, False),))
    frame = VhdlRecord("frame_t", (("samples", samples), ("int", VhdlBits(4))))
    assert ghdl._leaves("f", frame) == [
        "f.int_",
        "f.samples[1].im",
        "f.samples[1].re",
        "f.samples[0].im",
        "f.samples[0].re",
    ]


NETLIST = """\
module sub_Brtl_8
  (input  clk);
endmodule

module Demo
  (input  clk,
   input  [11:0] data,
   input  [7:0] \\bus_in[re] ,
   input  [7:0] \\bus_in[im] ,
   output [11:0] rows,
   output int);
  wire [11:0] total;
  wire n12;
  reg [3:0] n13;
  localparam n14 = 1'b1;
  assign rows = total;
endmodule
"""


def test_vhdl_wrapper():
    iq = VhdlRecord(
        "iq_t", (("re", VhdlBits(8, signed=True)), ("im", VhdlBits(8, signed=True)))
    )
    entity = VhdlEntity(
        "Demo",
        [
            VhdlGeneric("DW", "int", 12),
            VhdlGeneric("mode", "string", "add"),
            VhdlGeneric("invert", "bool", True),
            VhdlGeneric("scale", "real", 0.5),
            VhdlGeneric("init", None, None),
        ],
        [
            VhdlPort("clk", "input", VhdlBits(1, is_bit=True)),
            VhdlPort("data", "input", VhdlBits(12, signed=True)),
            VhdlPort("bus_in", "input", iq),
            VhdlPort(
                "rows", "output", VhdlArray(VhdlBits(6), (VhdlRange(1, 2, True),))
            ),
            VhdlPort("int", "output", VhdlBits(1, is_bit=True)),
        ],
    )
    text = ghdl.wrapper(entity, NETLIST, "a test")
    lines = [line.strip() for line in text.splitlines()]

    # The module has the name, the generics and the ports of the entity.
    assert "module Demo" in lines
    assert "import Demo_pkg::*;" in lines
    for parameter in (
        "parameter int DW = 12,",
        'parameter string mode = "add",',
        "parameter bit invert = 1'b1,",
        "parameter real scale = 0.5",
    ):
        assert parameter in lines
    assert "init" not in text
    for port in (
        "input  logic clk,",
        "input  logic signed [11:0] data,",
        "input  iq_t bus_in,",
        "output logic [5:0] rows [2],",
        # A name that is a keyword of SystemVerilog, C++ or Python gets an underscore.
        "output logic int_",
    ):
        assert port in lines
    assert "logic signed [7:0] re;" in lines

    # The netlist is renamed, and connected port by port: a record port is a port per element, an array
    # one vector.
    assert "module Demo_ghdl" in lines
    assert "Demo_ghdl impl_ (" in lines
    assert ".\\bus_in[re] (bus_in.re)," in lines
    assert "wire [11:0] flat_0_;" in lines
    assert "assign {rows[0], rows[1]} = flat_0_;" in lines
    assert ".\\rows (flat_0_)," in lines
    assert ".\\int (int_)" in lines

    # The nets that GHDL made up are not traced. Those of the design are.
    start = lines.index("/* verilator tracing_off */")
    end = lines.index("/* verilator tracing_on */")
    assert lines[start + 1 : end] == [
        "wire n12;",
        "reg [3:0] n13;",
        "localparam n14 = 1'b1;",
    ]
    assert lines.index("wire [11:0] total;") < start

    # The netlist must be that of the entity.
    wrong = replace_port(entity, "data", VhdlBits(10, signed=True))
    with pytest.raises(
        GhdlError, match="'data' of 'Demo' has 12 bits in the netlist of GHDL, and 10"
    ):
        ghdl.wrapper(wrong, NETLIST)
    with pytest.raises(GhdlError, match="has ports that the entity does not: int"):
        ghdl.wrapper(VhdlEntity("Demo", [], entity.ports[:-1]), NETLIST)
    with pytest.raises(GhdlError, match="did not synthesize the entity 'Other'"):
        ghdl.wrapper(VhdlEntity("Other", [], entity.ports), NETLIST)


def replace_port(entity: VhdlEntity, name: str, vhdl_type) -> VhdlEntity:
    ports = [
        VhdlPort(p.name, p.direction, vhdl_type if p.name == name else p.type)
        for p in entity.ports
    ]
    return VhdlEntity(entity.name, entity.generics, ports)


def test_ghdl_not_found(monkeypatch, tmp_path):
    monkeypatch.setenv("DSPSIM_GHDL", str(tmp_path / "missing"))
    assert not ghdl.ghdl_available()
    with pytest.raises(ghdl.GhdlNotFoundError, match="DSPSIM_GHDL is not a file"):
        build_vmodule("Missing", source=VHDL_DIR / "Skid.vhd", include_dirs=[])


@needs_ghdl
def test_vhdl_skid():
    """The VHDL skid buffer and the SystemVerilog one, with the same random stimulus."""

    @vbuilder(source=VHDL_DIR / "Skid.vhd")
    class SkidVhdl(VModule):
        # The generic of the entity.
        DW: int = 13
        clk: InputU8
        rst: InputU8
        # The port is a `signed` in VHDL.
        s_axis_tdata: InputS16
        s_axis_tvalid: InputU8
        s_axis_tready: OutputU8
        m_axis_tdata: OutputS16
        m_axis_tvalid: OutputU8
        m_axis_tready: InputU8

    SkidSv = build_vmodule(
        "SkidSv",
        source=HDL_DIR / "Skid.sv",
        include_dirs=[HDL_DIR],
        parameters={"DW": 13},
    )

    with Context() as context:
        clk = Clock("clk", 10)
        rst = SignalU8("rst", 1)
        tdata, tvalid, tready = (
            SignalS16("tdata"),
            SignalU8("tvalid"),
            SignalU8("tready"),
        )
        outputs = []
        for model in (SkidVhdl("vhdl"), SkidSv("sv")):
            assert model.DW == 13
            assert isinstance(model.s_axis_tdata, InputS16)
            assert model.s_axis_tdata.width == 13
            ready, valid, data = SignalU8("ready"), SignalU8("valid"), SignalS16("data")
            model.clk.bind(clk)
            model.rst.bind(rst)
            model.s_axis_tdata.bind(tdata)
            model.s_axis_tvalid.bind(tvalid)
            model.s_axis_tready.bind(ready)
            model.m_axis_tdata.bind(data)
            model.m_axis_tvalid.bind(valid)
            model.m_axis_tready.bind(tready)
            outputs.append((ready, valid, data))
        context.elaborate()
        context.run(30)
        rst.d = 0

        rng = random.Random(1)
        transfers = 0
        for _ in range(5000):
            tdata.d = rng.randrange(-4096, 4096)
            tvalid.d = rng.random() < 0.6
            tready.d = rng.random() < 0.5
            context.run(10)
            (ready, valid, data), (sv_ready, sv_valid, sv_data) = outputs
            assert (ready.q, valid.q) == (sv_ready.q, sv_valid.q)
            if valid.q:
                assert data.q == sv_data.q
                transfers += tready.q
        assert transfers > 1000


def signals_for(port, name: str):
    """The signal, signal array or struct of signals that a port binds to."""
    if isinstance(port, PortStruct | PortStructArray):
        return port.signals(name)
    # InputArrayU8 binds to a SignalArrayU8, OutputS16 to a SignalS16.
    cls = getattr(framework, re.sub("^(Input|Output)", "Signal", type(port).__name__))
    return cls(name, port.shape) if hasattr(port, "shape") else cls(name, port.width)


def changed_bus(bus: dict, n: int) -> dict:
    """What VhdlModel does to a bus: the function `changed` of its architecture."""

    def signed(value: int, bits: int) -> int:
        value &= (1 << bits) - 1
        return value - (1 << bits) if value >> (bits - 1) else value

    return {
        "data": signed(bus["data"] + n, 16),
        "valid": bus["valid"],
        "id": (bus["id"] + 1) % 8,
        "level": -bus["level"],
        "state": (bus["state"] + 1) % 5,
        "last": 1 - bus["last"],
        "re": bus["im"],
        "im": signed(bus["re"] + 1, 8),
        "taps": [(bus["taps"][3 - i] + i) % 64 for i in range(4)],
    }


def random_bus(rng: random.Random) -> dict:
    return {
        "data": rng.randrange(-30000, 30000),
        "valid": rng.randrange(2),
        "id": rng.randrange(8),
        "level": rng.randrange(-5, 6),
        "state": rng.randrange(5),
        "last": rng.randrange(2),
        "re": rng.randrange(-128, 128),
        "im": rng.randrange(-128, 128),
        "taps": [rng.randrange(64) for _ in range(4)],
    }


def write_bus(signals, bus: dict) -> None:
    signals.data.d = bus["data"]
    signals.valid.d = bus["valid"]
    signals.id.d = bus["id"]
    signals.level.d = bus["level"]
    signals.state.d = bus["state"]
    signals.last.d = bus["last"]
    signals.iq.re.d = bus["re"]
    signals.iq.im.d = bus["im"]
    signals.taps.d = bus["taps"]


def read_bus(signals) -> dict:
    return {
        "data": signals.data.q,
        "valid": signals.valid.q,
        "id": signals.id.q,
        "level": signals.level.q,
        "state": signals.state.q,
        "last": signals.last.q,
        "re": signals.iq.re.q,
        "im": signals.iq.im.q,
        "taps": list(signals.taps.q),
    }


@needs_ghdl
def test_vhdl_model_interface():
    """Generics are parameters, and every port has the type of its VHDL type."""

    @vbuilder(source=VHDL_DIR / "VhdlModel.vhd", vhdl_sources=VHDL_MODEL_SOURCES)
    class VhdlModel(VModule):
        clk: InputU8
        rst: InputU8
        data_in: InputS16
        data_out: OutputS16
        acc_out: OutputS16
        wide_in: InputArrayU32
        wide_out: OutputArrayU32
        count_in: InputU8
        count_out: OutputS8
        state_in: InputU8
        state_out: OutputU8
        flag_in: InputU8
        flag_out: OutputU8
        vec_in: InputArrayU8
        vec_out: OutputArrayU8
        mat_in: InputArrayS8
        mat_out: OutputS16
        bus_in: PortStruct
        bus_out: PortStruct
        buses_in: PortStructArray
        buses_out: PortStructArray
        frame_in: PortStruct
        frame_out: PortStruct

    # The generics, with the names and the values of the entity. A boolean is 0 or 1.
    assert (VhdlModel.DW, VhdlModel.N, VhdlModel.OFFSET) == (12, 3, -2)
    assert (VhdlModel.MODE, VhdlModel.INVERT, VhdlModel.SCALE) == ("add", 0, 0.5)

    with Context():
        dut = VhdlModel("dut")
        # Vectors: signed and unsigned, with the widths that the generics give.
        assert (dut.data_in.width, dut.acc_out.width) == (12, 16)
        assert dut.wide_in.shape == (4,)
        # Integers have the bits of their range, an enumeration those of its values.
        assert (dut.count_in.width, dut.count_out.width) == (7, 8)
        assert (dut.state_in.width, dut.flag_in.width) == (3, 1)
        # Arrays are arrays, one dimension for each of the VHDL type.
        assert (dut.vec_in.shape, dut.vec_out.shape) == ((3,), (3,))
        assert dut.mat_in.shape == (2, 3)
        assert dut.mat_in[1, 2].width == 8
        # Records are structs, with their records and arrays inside.
        assert dut.bus_in.fields == [
            "data",
            "valid",
            "id",
            "level",
            "state",
            "last",
            "iq",
            "taps",
        ]
        assert isinstance(dut.bus_in.data, InputS16)
        assert isinstance(dut.bus_in.level, InputS8)
        assert dut.bus_in.level.width == 4
        assert isinstance(dut.bus_in.iq.re, InputS8)
        assert dut.bus_in.taps.shape == (4,)
        assert dut.buses_in.shape == (2,)
        assert dut.buses_in[1].iq.im.name == "buses_in[1].iq.im"
        assert dut.frame_in.fields == ["samples", "count"]
        assert isinstance(dut.frame_in.samples, PortStructArray)
        assert dut.frame_out.samples[1].taps.shape == (4,)


@needs_ghdl
@pytest.mark.parametrize(
    "generics",
    [{}, {"DW": 9, "N": 5, "OFFSET": 7, "MODE": "sub", "INVERT": 1}],
    ids=["defaults", "overrides"],
)
def test_vhdl_model(generics):
    """Every port of every kind carries the values that the VHDL computes, for two sets of generics."""
    VhdlModel = build_vmodule(
        "VhdlModel",
        source=VHDL_DIR / "VhdlModel.vhd",
        include_dirs=[],
        parameters=generics,
        vhdl_sources=VHDL_MODEL_SOURCES,
    )
    dw, n = generics.get("DW", 12), generics.get("N", 3)
    offset = generics.get("OFFSET", -2) * (
        1 if generics.get("MODE", "add") == "add" else -1
    )
    invert = generics.get("INVERT", 0)
    assert (VhdlModel.DW, VhdlModel.N, VhdlModel.INVERT) == (dw, n, invert)
    assert VhdlModel.MODE == generics.get("MODE", "add")

    def signed(value: int, bits: int) -> int:
        value &= (1 << bits) - 1
        return value - (1 << bits) if value >> (bits - 1) else value

    with Context() as context:
        clk = Clock("clk", 10)
        dut = VhdlModel("dut")
        dut.clk.bind(clk)
        assert dut.data_in.width == dw
        assert dut.vec_in.shape == (n,)

        rst = SignalU8("rst", 1)
        dut.rst.bind(rst)
        signals = {
            name: signals_for(getattr(dut, name), name) for name in VHDL_MODEL_PORTS
        }
        for name, made in signals.items():
            getattr(dut, name).bind(made)
        acc_out = signals["acc_out"]
        context.elaborate()
        context.run(20)
        rst.d = 0

        rng = random.Random(4)
        acc = 0
        for _ in range(200):
            data = rng.randrange(-(1 << (dw - 1)), 1 << (dw - 1))
            wide = rng.getrandbits(100)
            count, state, flag = rng.randrange(101), rng.randrange(5), rng.randrange(2)
            vec = [rng.randrange(256) for _ in range(n)]
            mat = [[rng.randrange(-128, 128) for _ in range(3)] for _ in range(2)]
            bus = random_bus(rng)
            buses = [random_bus(rng) for _ in range(2)]
            samples = [random_bus(rng) for _ in range(2)]
            frame_count = rng.randrange(101)

            signals["data_in"].d = data
            signals["wide_in"].d = to_words(wide, 4)
            signals["count_in"].d = count
            signals["state_in"].d = state
            signals["flag_in"].d = flag
            signals["vec_in"].d = vec
            signals["mat_in"].d = mat
            write_bus(signals["bus_in"], bus)
            for i in range(2):
                write_bus(signals["buses_in"][i], buses[i])
                write_bus(signals["frame_in"].samples[i], samples[i])
            signals["frame_in"].count.d = frame_count
            context.run(10)

            acc = signed(acc + data, dw + 4)
            assert signals["data_out"].q == signed(data + offset, dw)
            assert acc_out.q == acc
            assert from_words(signals["wide_out"].q) == wide ^ ((1 << 100) - 1)
            assert signals["count_out"].q == -count
            assert signals["state_out"].q == (state + 1) % 5
            assert signals["flag_out"].q == flag ^ invert
            # vec_in is (1 to N) and vec_out is (N-1 downto 0): element i is the i-th from the lowest index.
            assert list(signals["vec_out"].q) == [(vec[i] + i) % 256 for i in range(n)]
            weighted = sum(
                mat[i][j] * (3 * i + j + 1) for i in range(2) for j in range(3)
            )
            assert signals["mat_out"].q == signed(weighted, 12)
            assert read_bus(signals["bus_out"]) == changed_bus(bus, 0)
            for i in range(2):
                assert read_bus(signals["buses_out"][i]) == changed_bus(
                    buses[i], 10 * (i + 1)
                )
                assert read_bus(signals["frame_out"].samples[i]) == changed_bus(
                    samples[i], 100 * (i + 1)
                )
            assert signals["frame_out"].count.q == 100 - frame_count


@needs_ghdl
def test_vhdl_generic_errors():
    def build(parameters):
        return build_vmodule(
            "VhdlModelErrors",
            source=VHDL_DIR / "VhdlModel.vhd",
            include_dirs=[],
            parameters=parameters,
            vhdl_sources=VHDL_MODEL_SOURCES,
        )

    # GHDL cannot set a generic of type real.
    with pytest.raises(GhdlError, match="generic 'SCALE' of 'VhdlModel' is a real"):
        build({"SCALE": 0.25})
    # A value that the design does not elaborate with.
    with pytest.raises(GhdlError, match="could not elaborate"):
        build({"DW": 0})
    # A file of the design is missing.
    with pytest.raises(GhdlError, match="vhdl_model_pkg"):
        build_vmodule(
            "VhdlModelAlone", source=VHDL_DIR / "VhdlModel.vhd", include_dirs=[]
        )
    with pytest.raises(FileNotFoundError, match="VHDL source not found"):
        build_vmodule("VhdlMissing", source=VHDL_DIR / "Missing.vhd", include_dirs=[])


@needs_ghdl
def test_vhdl_trace(tmp_path):
    """The trace has the signals of the VHDL design, in the hierarchy of its entities."""
    VhdlModel = build_vmodule(
        "VhdlModelTrace",
        source=VHDL_DIR / "VhdlModel.vhd",
        include_dirs=[],
        trace="vcd",
        vhdl_sources=VHDL_MODEL_SOURCES,
    )
    with Context() as context:
        clk = Clock("clk", 10)
        dut = VhdlModel("dut")
        dut.clk.bind(clk)
        dut.rst.bind(SignalU8("rst"))
        for port in VHDL_MODEL_PORTS:
            getattr(dut, port).bind(signals_for(getattr(dut, port), port))
        dut.open_trace(tmp_path / "vhdl.vcd")
        context.elaborate()
        context.run(40)
        dut.close_trace()

    trace = (tmp_path / "vhdl.vcd").read_text()
    names = [
        line.split()[4]
        for line in trace.splitlines()
        if line.strip().startswith("$var")
    ]
    scopes = [
        line.split()[2]
        for line in trace.splitlines()
        if line.strip().startswith("$scope")
    ]
    # The entity, the netlist inside it, and the instance of the other entity.
    assert {"VhdlModel", "impl_", "u_acc"} <= set(scopes)
    # The signal of the accumulator, and the fields of a record port.
    assert "sum" in names
    assert "bus_in" in scopes and "data" in names
    # None of the nets that GHDL made up for expressions.
    assert not [name for name in names if name[0] == "n" and name[1:].isdigit()]


@needs_ghdl
def test_vhdl_project(tmp_path):
    """A VHDL model in the [tool.dspsim] table of a library."""
    (tmp_path / "pyproject.toml").write_text(f"""
[tool.dspsim]
name = "library"
sources = ["{VHDL_DIR.as_posix()}/VhdlModel.vhd", "{VHDL_DIR.as_posix()}/Skid.vhd", "{HDL_DIR.as_posix()}/Skid.sv"]
exclude_sources = ["{HDL_DIR.as_posix()}/Skid.sv"]
# The other VHDL files of the designs.
vhdl_sources = ["{VHDL_DIR.as_posix()}/vhdl_model_pkg.vhd", "{VHDL_DIR.as_posix()}/Accumulator.vhd"]

[tool.dspsim.parameters]
DW = 10

[tool.dspsim.models.VhdlModel.parameters]
N = 2
MODE = "sub"
""")
    config = ProjectConfig.from_toml(
        tmp_path / "pyproject.toml", tmp_path / "generated"
    )
    assert list(config.models) == ["VhdlModel", "Skid"]

    model = config.models["VhdlModel"]
    # The source of the model is the SystemVerilog that was written for these values of the generics.
    assert model.source.suffix == ".sv"
    assert model.source.is_relative_to(tmp_path / "generated" / "vhdl")
    assert {name: p.value for name, p in model.parameters.items()} == {
        "DW": 10,
        "N": 2,
        "OFFSET": -2,
        "MODE": "sub",
        "INVERT": 0,
        "SCALE": 0.5,
    }
    assert model.ports["data_in"].width == 10 and model.ports["data_in"].signed
    assert model.ports["vec_in"].shape == (2,)
    assert model.ports["bus_in"].is_struct
    assert config.models["Skid"].ports["s_axis_tdata"].width == 10


@needs_ghdl
def test_vhdl_library(tmp_path):
    """Build a library with VHDL models with CMake, and use it."""
    (tmp_path / "pyproject.toml").write_text(f"""
[tool.dspsim]
name = "vhdl_library"
sources = ["{VHDL_DIR.as_posix()}/VhdlModel.vhd", "{VHDL_DIR.as_posix()}/Skid.vhd"]
vhdl_sources = ["{VHDL_DIR.as_posix()}/vhdl_model_pkg.vhd", "{VHDL_DIR.as_posix()}/Accumulator.vhd"]

[tool.dspsim.models.Skid.parameters]
DW = 9
""")
    (tmp_path / "CMakeLists.txt").write_text("""
cmake_minimum_required(VERSION 4.0)
project(vhdl_library_test LANGUAGES CXX)

find_package(dspsim REQUIRED)
dspsim_add_module(vhdl_library ${CMAKE_SOURCE_DIR}/pyproject.toml vhdl_library_test)
""")
    build_dir = tmp_path / "build"
    build_module(tmp_path, build_dir, verbose=True)
    (module_path,) = [
        path
        for pattern in ("**/vhdl_library*.so", "**/vhdl_library*.pyd")
        for path in build_dir.glob(pattern)
    ]
    spec = importlib.util.spec_from_file_location("vhdl_library", module_path)
    assert spec is not None and spec.loader is not None
    library = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(library)
    dspsim.link_module(library)

    assert library.Skid.DW == 9
    assert (library.VhdlModel.DW, library.VhdlModel.MODE) == (12, "add")
    with Context() as context:
        clk = Clock("clk", 10)
        skid = library.Skid("skid")
        assert isinstance(skid.s_axis_tdata, InputS16)
        assert skid.s_axis_tdata.width == 9

        dut = library.VhdlModel("dut")
        dut.clk.bind(clk)
        dut.rst.bind(SignalU8("rst"))
        signals = {
            name: signals_for(getattr(dut, name), name) for name in VHDL_MODEL_PORTS
        }
        for name, made in signals.items():
            getattr(dut, name).bind(made)
        for name in ("clk", "rst", "s_axis_tdata", "s_axis_tvalid", "s_axis_tready"):
            getattr(skid, name).bind(signals_for(getattr(skid, name), f"skid_{name}"))
        for name in ("m_axis_tdata", "m_axis_tvalid", "m_axis_tready"):
            getattr(skid, name).bind(signals_for(getattr(skid, name), f"skid_{name}"))
        context.elaborate()

        bus = random_bus(random.Random(5))
        write_bus(signals["bus_in"], bus)
        signals["data_in"].d = 100
        context.run(10)
        assert signals["data_out"].q == 98
        assert read_bus(signals["bus_out"]) == changed_bus(bus, 0)


@needs_ghdl
def test_vhdl_transpile_command(tmp_path):
    """python -m dspsim.ghdl writes the SystemVerilog of an entity and prints its path."""
    command = [
        sys.executable,
        "-m",
        "dspsim.ghdl",
        str(VHDL_DIR / "Skid.vhd"),
        "--output-dir",
        str(tmp_path),
        "-g",
        "DW=20",
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=True)
    path = Path(result.stdout.strip())
    assert path.name == "Skid.sv" and path.is_relative_to(tmp_path)
    text = path.read_text()
    assert "parameter int DW = 20" in text
    assert "input  logic signed [19:0] s_axis_tdata," in text
    # The same entity with the same generics is not transpiled again.
    again = subprocess.run(command, capture_output=True, text=True, check=True)
    assert Path(again.stdout.strip()) == path

    failed = subprocess.run(
        [*command, "-g", "MISSING=1"], capture_output=True, text=True, check=False
    )
    assert failed.returncode != 0
    assert "'Skid' has no generic 'MISSING'" in failed.stderr
