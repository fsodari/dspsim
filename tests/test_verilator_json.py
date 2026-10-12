import time
from pathlib import Path

import pytest

from dspsim.verilator import parse_module_json, verilate_json

HDL_DIR = Path(__file__).parent.parent / "hdl"


def test_verilate_json():
    print()
    # Example test for verilate_json function
    source = HDL_DIR / "HellModel.sv"
    odir = Path("build")
    json_output, metadata = verilate_json(
        [source],
        output_dir=odir,
        include_dirs=[HDL_DIR],
    )

    start_time = time.time()
    module_info = parse_module_json(json_output, metadata)
    elapsed_time = time.time() - start_time
    print(f"Parsing module JSON took {elapsed_time:.6f} seconds")

    print(module_info)
    for parameter in module_info.parameters.values():
        print(parameter)
    for port in module_info.ports.values():
        print(port)


def test_signed_ports():
    """Signed HDL ports are detected and select the signed vport macros."""
    json_output, metadata = verilate_json(
        [HDL_DIR / "HellModel.sv"], include_dirs=[HDL_DIR]
    )
    ports = parse_module_json(json_output, metadata).ports

    expected = {
        "a": (False, "DSPSIM_VINPUT"),
        "b": (False, "DSPSIM_VOUTPUT"),
        "c": (True, "DSPSIM_VINPUT_S"),
        "d": (True, "DSPSIM_VOUTPUT_S"),
        "e": (True, "DSPSIM_VOUTPUT_ARRAY_S"),
        "f": (True, "DSPSIM_VOUTPUT_ARRAY_S"),
        "g": (False, "DSPSIM_VOUTPUT_ARRAY"),
    }
    for name, (signed, macro) in expected.items():
        assert ports[name].signed == signed, name
        assert ports[name].vmacro == macro, name


def test_wide_ports():
    """Ports of more than 64 bits are arrays of 32-bit words: the words are the last dimension."""
    source = HDL_DIR / "WideModel.sv"
    json_output, metadata = verilate_json([source], include_dirs=[HDL_DIR])
    ports = parse_module_json(json_output, metadata).ports

    # name: (width, wide, shape of the dspsim port, macro)
    expected = {
        "clk": (1, False, (), "DSPSIM_VINPUT"),
        "a": (100, True, (4,), "DSPSIM_VINPUT_WIDE"),
        "sum": (101, True, (4,), "DSPSIM_VOUTPUT_WIDE"),
        # The words of a signed port are unsigned.
        "s": (100, True, (4,), "DSPSIM_VINPUT_WIDE"),
        "neg": (100, True, (4,), "DSPSIM_VOUTPUT_WIDE"),
        "arr_in": (128, True, (3, 4), "DSPSIM_VINPUT_WIDE"),
        "arr_out": (128, True, (3, 4), "DSPSIM_VOUTPUT_WIDE"),
    }
    for name, (width, wide, shape, macro) in expected.items():
        port = ports[name]
        assert port.width == width, name
        assert port.is_wide == wide, name
        assert port.port_shape == shape, name
        assert port.is_port_array == bool(shape), name
        assert port.vmacro == macro, name
        assert not port.is_signed_int, name
    assert ports["s"].signed
    assert ports["a"].stdint_size == 32
    assert ports["arr_in"].shape == (3,)


def test_struct_ports():
    """A struct port has a field for each member of the struct, and a field can be a struct."""
    source = HDL_DIR / "StructModel.sv"
    json_output, metadata = verilate_json([source], include_dirs=[HDL_DIR])
    ports = parse_module_json(json_output, metadata).ports
    assert list(ports) == ["clk", "u_in", "u_out", "p_in", "p_out", "w_in", "w_out"]
    assert not ports["clk"].is_struct

    # Unpacked: Verilator keeps a variable per field.
    bus = ports["u_out"]
    assert bus.is_struct and not bus.packed
    assert list(bus.fields) == ["data", "valid", "taps", "iq", "wide", "flags"]
    assert all(f.direction == "output" for f in bus.fields.values())
    assert (bus.fields["data"].width, bus.fields["data"].signed) == (12, True)
    assert bus.fields["data"].vfield_type() == "::dspsim::VOutput<int16_t, 12>"
    assert bus.fields["data"].vl_member == "__PVT__data"
    assert bus.fields["taps"].shape == (4,)
    assert bus.fields["taps"].vfield_type() == "::dspsim::VOutputArray<uint8_t, 6>"
    assert bus.fields["wide"].vfield_type() == "::dspsim::VOutputWide<100>"
    iq = bus.fields["iq"]
    assert iq.is_struct and not iq.packed and list(iq.fields) == ["re", "im"]
    assert iq.vfield_type() == "iq_struct"
    # A packed struct in an unpacked one is a vector of its own.
    flags = bus.fields["flags"]
    assert flags.is_struct and flags.packed
    assert (flags.width, flags.storage_ctype) == (4, "uint8_t")

    # Packed: one vector, with the first field in the most significant bits.
    packed = ports["p_in"]
    assert packed.is_struct and packed.packed
    assert (packed.width, packed.storage_ctype) == (17, "uint32_t")
    assert {name: (f.lsb, f.width) for name, f in packed.fields.items()} == {
        "data": (5, 12),
        "valid": (4, 1),
        "flags": (0, 4),
    }
    assert (
        packed.fields["data"].vfield_type("uint32_t")
        == "::dspsim::VInputBits<int16_t, 12, uint32_t>"
    )
    assert {name: f.lsb for name, f in packed.fields["flags"].fields.items()} == {
        "id": 1,
        "last": 0,
    }

    # Packed and more than 64 bits: 32-bit words. A field of more than 64 bits is an array of words.
    wide = ports["w_in"]
    assert (wide.width, wide.storage_ctype) == (147, "uint32_t")
    assert {name: (f.lsb, f.width) for name, f in wide.fields.items()} == {
        "wide": (47, 100),
        "mid": (7, 40),
        "flags": (3, 4),
        "lo": (0, 3),
    }
    assert (
        wide.fields["wide"].vfield_type("uint32_t") == "::dspsim::VInputWideBits<100>"
    )
    assert (
        wide.fields["mid"].vfield_type("uint32_t")
        == "::dspsim::VInputBits<int64_t, 40, uint32_t>"
    )


TYPES_SV = """
package types_pkg;
  typedef enum logic [1:0] {IDLE, RUN, DONE} state_t;
  typedef logic [3:0][7:0] quad_t;
  typedef logic [7:0] row_t [3];
  typedef struct packed {
    state_t          state;
    quad_t           quad;
    logic [1:0][2:0] pairs;
  } packed_t;
  typedef struct {
    state_t state;
    row_t   row;
  } unpacked_t;
endpackage

module Types import types_pkg::*; (
  input  state_t           state,
  input  quad_t            quad,
  input  row_t             row,
  input  logic [11:0][9:0] big,
  input  int               count,
  input  packed_t          p,
  output unpacked_t        u
);
  assign u.state = state;
  assign u.row = row;
endmodule
"""


def test_port_types(tmp_path):
    """Typedefs, enumerations and packed arrays are the bits of the type that they are made of."""
    source = tmp_path / "Types.sv"
    source.write_text(TYPES_SV)
    json_output, metadata = verilate_json([source], include_dirs=[tmp_path])
    ports = parse_module_json(json_output, metadata).ports

    def described(port):
        return (port.keyword, port.width, port.signed, port.shape, port.lsb)

    # An enumeration is its base type, and a packed array one vector.
    assert described(ports["state"]) == ("logic", 2, False, (), 0)
    assert described(ports["quad"]) == ("logic", 32, False, (), 0)
    assert ports["quad"].vmacro == "DSPSIM_VINPUT"
    # The shape of an unpacked array comes through its typedef.
    assert described(ports["row"]) == ("logic", 8, False, (3,), 0)
    # A packed array of more than 64 bits is a wide port.
    assert described(ports["big"]) == ("logic", 120, False, (), 0)
    assert ports["big"].port_shape == (4,)
    assert described(ports["count"]) == ("int", 32, True, (), 0)

    packed = ports["p"]
    assert (packed.packed, packed.width, packed.storage_ctype) == (True, 40, "uint64_t")
    assert {name: described(f) for name, f in packed.fields.items()} == {
        "state": ("logic", 2, False, (), 38),
        "quad": ("logic", 32, False, (), 6),
        "pairs": ("logic", 6, False, (), 0),
    }
    unpacked = ports["u"]
    assert (unpacked.packed, unpacked.direction) == (False, "output")
    assert {name: described(f) for name, f in unpacked.fields.items()} == {
        "state": ("logic", 2, False, (), 0),
        "row": ("logic", 8, False, (3,), 0),
    }


def test_struct_array_ports():
    """An array of structs has the dimensions of the array, and the fields of one struct."""
    source = HDL_DIR / "StructArrayModel.sv"
    json_output, metadata = verilate_json([source], include_dirs=[HDL_DIR])
    ports = parse_module_json(json_output, metadata).ports

    # 2 x 3 unpacked structs: each a C++ struct of the verilated model.
    array = ports["a_in"]
    assert (array.is_struct, array.packed, array.shape) == (True, False, (2, 3))
    assert array.vfield_type() == "::dspsim::PortStructArrayOf<a_in_struct>"
    assert array.vport_args == (
        "::dspsim::Shape{2, 3}, ::dspsim::detail::vl_structs<a_in_struct>"
        "(::dspsim::detail::vl_element_pointers(top->a_in))"
    )
    # The fields are those of one struct: the array does not change them.
    data, taps, wide = (array.fields[name] for name in ("data", "taps", "wide"))
    assert (data.shape, taps.shape, wide.port_shape) == ((), (2,), (4,))
    assert data.vfield_type() == "::dspsim::VInput<int16_t, 12>"
    assert data.vfield_args(array) == "struct_ext.__PVT__data"
    assert array.fields["iq"].vfield_type() == "iq_struct"

    # Packed structs: one vector each, of 32-bit words above 64 bits.
    packed = ports["w_out"]
    assert (packed.packed, packed.shape, packed.width) == (True, (2,), 86)
    assert packed.vport_args == (
        "::dspsim::Shape{2}, ::dspsim::detail::vl_structs<w_out_struct>"
        "(::dspsim::detail::vl_word_pointers<uint32_t>(top->w_out, 2))"
    )
    assert (
        packed.fields["wide"].vfield_type("uint32_t") == "::dspsim::VOutputWideBits<70>"
    )
    assert packed.fields["data"].vfield_args(packed) == "struct_ext, struct_lsb + 4"

    # Arrays of structs as fields of a struct.
    outer = ports["o_in"]
    assert (outer.shape, outer.vfield_type(), outer.vport_args) == (
        (),
        "o_in_struct",
        "top->o_in",
    )
    iqs, flags = outer.fields["iqs"], outer.fields["flags"]
    assert (iqs.shape, flags.shape) == ((3,), (2,))
    assert iqs.vfield_type() == "::dspsim::PortStructArrayOf<iqs_struct>"
    assert iqs.vfield_args(outer) == (
        "::dspsim::Shape{3}, ::dspsim::detail::vl_structs<iqs_struct>"
        "(::dspsim::detail::vl_element_pointers(struct_ext.__PVT__iqs))"
    )
    assert flags.vfield_args(outer) == (
        "::dspsim::Shape{2}, ::dspsim::detail::vl_structs<flags_struct>"
        "(::dspsim::detail::vl_word_pointers<uint8_t>(struct_ext.__PVT__flags, 2))"
    )
    assert outer.fields["valid"].vfield_args(outer) == "struct_ext.__PVT__valid"

    # A packed union and a packed array of structs are the vectors of their bits.
    union, packed_array = ports["u_in"], ports["pk_out"]
    assert (union.is_struct, union.keyword, union.width) == (False, "logic", 16)
    assert union.vmacro == "DSPSIM_VINPUT"
    assert (packed_array.is_struct, packed_array.width) == (False, 12)


def test_unsupported_port_types(tmp_path):
    """A type that has no port is reported with the name of the port."""
    source = tmp_path / "Union.sv"
    source.write_text("""
package union_pkg;
  typedef union { logic [7:0] a; logic [7:0] b; } unpacked_union_t;
  typedef struct { logic valid; unpacked_union_t value; } bus_t;
endpackage
module Union import union_pkg::*; (input bus_t bus, output logic [7:0] y);
  assign y = bus.value.a;
endmodule
""")
    json_output, metadata = verilate_json([source], include_dirs=[tmp_path])
    with pytest.raises(
        ValueError, match="Port 'bus.value': unpacked unions are not supported"
    ):
        parse_module_json(json_output, metadata)
