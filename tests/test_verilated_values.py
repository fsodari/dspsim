"""
The struct ports of verilated modules as values: one port with the struct as its value, which is what a
struct port is by default. The models and what they do to their fields are those of the tests with a port
per field (struct_ports="fields") in test_builder.py.

The names of the models are short: see the note on names in CLAUDE.md.
"""

import random
from pathlib import Path

import numpy as np
import pytest

from dspsim.builder import VModule, build_vmodule, vbuilder
from dspsim.framework import (
    Clock,
    Context,
    FieldSignalS16,
    InputS16,
    InputU8,
    InputU16,
    Module,
    OutputU16,
    PortBase,
    PortBaseArrayView,
    PortStruct,
    Record,
    SignalBase,
    SignalBaseArrayView,
    SignalU16,
    from_words,
    to_words,
)
from dspsim.project import load_model_info

HDL_DIR = Path(__file__).parent.parent / "hdl"

IQ = np.dtype([("re", "i1"), ("im", "i1")])
FLAGS = np.dtype([("id", "u1"), ("last", "u1")])
STRUCT_PORTS = ("u_in", "u_out", "p_in", "p_out", "w_in", "w_out")


def connect(dut, names) -> dict[str, SignalBase | SignalBaseArrayView]:
    """Bind each of the ports to a signal that it creates."""
    signals = {}
    for name in names:
        port = getattr(dut, name)
        if isinstance(port, PortBaseArrayView):
            signals[name] = port.signals(name)
        else:
            signals[name] = port.signal(name)
        port.bind(signals[name])
    return signals


def test_value_struct_info():
    """The C++ struct of a value is named after the type in the source and the fields that it has."""
    info = load_model_info(HDL_DIR / "StructModel.sv")
    assert info.struct_ports == "value"
    structs = {s.type_name.rpartition("::")[2]: s for s in info.value_structs}
    # Each struct once, after the structs of its fields.
    assert list(structs) == [
        "iq_t",
        "flags_t",
        "bus_t",
        "packed_bus_t",
        "packed_wide_t",
    ]
    bus = structs["bus_t"]
    assert bus.value_class.startswith("bus_t_")
    assert info.ports["u_in"].value_class == info.ports["u_out"].value_class
    assert {name: f.value_ctype for name, f in bus.fields.items()} == {
        "data": "int16_t",
        "valid": "uint8_t",
        "taps": "std::array<uint8_t, 4>",
        "iq": f"::dspsim::generated::{structs['iq_t'].value_class}",
        "wide": "std::array<uint32_t, 4>",
        "flags": f"::dspsim::generated::{structs['flags_t'].value_class}",
    }
    # The same struct type in another module is the same C++ struct, so that their ports bind to one signal.
    other = load_model_info(HDL_DIR / "StructArrayModel.sv")
    flags = {s.type_name.rpartition("::")[2]: s for s in other.value_structs}["flags_t"]
    assert flags.value_class == structs["flags_t"].value_class
    # An array of structs is an array of ports with the struct as their value.
    assert other.ports["a_in"].value_ctype.startswith("std::array<std::array<")
    assert (
        load_model_info(HDL_DIR / "StructModel.sv", struct_ports="fields").value_structs
        == []
    )
    with pytest.raises(ValueError, match="struct_ports is 'value' or 'fields'"):
        load_model_info(HDL_DIR / "StructModel.sv", struct_ports="ports")  # type: ignore[arg-type]


def test_value_struct_ports():
    """A struct port is one port, read and written as a numpy record: unpacked, packed, and packed and wide."""

    @vbuilder(source=HDL_DIR / "StructModel.sv", include_dirs=[HDL_DIR])
    class StructValue(VModule):
        clk: InputU8
        u_in: PortBase
        u_out: PortBase
        p_in: PortBase
        p_out: PortBase
        w_in: PortBase
        w_out: PortBase

    with Context() as context:
        clk = Clock("clk", 10)
        dut = StructValue("dut")
        assert type(dut.u_in) is PortBase
        assert dut.u_in.name == "u_in"
        assert not dut.u_in.is_output and dut.u_out.is_output
        # The fields have the types of the ports that they would be: 12 bits in an int16, 100 bits in words.
        assert dut.u_in.dtype.names == ("data", "valid", "taps", "iq", "wide", "flags")
        assert dut.u_in.dtype["data"] == np.dtype("<i2")
        assert dut.u_in.dtype["taps"] == np.dtype(("u1", (4,)))
        assert dut.u_in.dtype["iq"] == IQ
        assert dut.u_in.dtype["wide"] == np.dtype(("<u4", (4,)))
        assert dut.u_in.dtype["flags"] == FLAGS
        assert dut.p_in.dtype.names == ("data", "valid", "flags")
        assert dut.w_in.dtype["mid"] == np.dtype("<i8")

        s = connect(dut, STRUCT_PORTS)
        dut.clk.bind(clk)
        assert type(s["u_in"]) is SignalBase
        assert s["u_in"].fields == ["data", "valid", "taps", "iq", "wide", "flags"]
        context.elaborate()

        rng = random.Random(2)
        all_ones = (1 << 100) - 1
        for _ in range(200):
            data, re, im = (
                rng.randrange(-2047, 2048),
                rng.randrange(-127, 128),
                rng.randrange(-127, 128),
            )
            taps = [rng.randrange(64) for _ in range(4)]
            wide, ident, last = rng.getrandbits(100), rng.randrange(8), rng.randrange(2)
            # Unpacked struct, as a tuple with the fields in order.
            s["u_in"].write(
                (data, last, taps, (re, im), to_words(wide, 4), (ident, last))
            )
            # Packed struct, as a mapping.
            p_data, p_id = rng.randrange(-2047, 2048), rng.randrange(8)
            s["p_in"].write(
                {"data": p_data, "valid": last, "flags": {"id": p_id, "last": last}}
            )
            # Packed struct of more than 64 bits, as a record that is changed and written back.
            w_wide, mid, w_id, lo = (
                rng.getrandbits(100),
                rng.randrange(-(2**39) + 1, 2**39),
                rng.randrange(8),
                rng.randrange(8),
            )
            w = s["w_in"].read()
            w.wide = to_words(w_wide, 4)
            w.mid = mid
            w.flags.id = w_id
            w.flags.last = last
            w.lo = lo
            s["w_in"].write(w)
            context.run(10)

            u = s["u_out"].read()
            assert isinstance(u, Record)
            assert u.data == -data
            assert u.valid == last
            assert u.taps.tolist() == taps[::-1]
            assert (u.iq.re, u.iq.im) == (im, re)
            assert from_words(u.wide) == wide ^ all_ones
            assert (u.flags.id, u.flags.last) == ((ident + 1) % 8, 1 - last)

            p = dut.p_out.read()
            assert p.data == -p_data
            assert p.valid == last
            assert (p.flags.id, p.flags.last) == ((p_id + 2) % 8, 1 - last)

            w = s["w_out"].value
            assert from_words(w.wide) == w_wide ^ all_ones
            assert w.mid == -mid
            assert (w.flags.id, w.flags.last, w.lo) == (
                (w_id + 3) % 8,
                last,
                (lo + 1) % 8,
            )

        # Values outside of the bits of a field are cut to them on the way to the model: data has 12 bits.
        s["u_in"].write(
            (0x7FFF, 0xFF, [0xFF] * 4, (1, 1), [0xFFFFFFFF] * 4, (0xFF, 0xFF))
        )
        context.run(10)
        u = s["u_out"].read()
        assert u.data == 1
        assert u.valid == 1
        assert u.taps.tolist() == [63] * 4
        assert from_words(u.wide) == 0
        assert (u.flags.id, u.flags.last) == (0, 0)


def test_value_struct_between_modules():
    """
    Two modules connect through one signal per struct. They are two builds, each with its own extension module:
    a struct type is the same C++ type in both.
    """
    First = build_vmodule(
        "ValueFirst", source=HDL_DIR / "StructModel.sv", include_dirs=[HDL_DIR]
    )
    Second = build_vmodule(
        "ValueSecond", source=HDL_DIR / "StructModel.sv", include_dirs=[HDL_DIR]
    )
    assert First is not Second
    with Context() as context:
        clk = Clock("clk", 10)
        a = First("a")
        b = Second("b")
        buses = {}
        for kind in "upw":
            first = getattr(a, f"{kind}_in").signal(f"{kind}_first")
            middle = getattr(a, f"{kind}_out").signal(f"{kind}_middle")
            last = getattr(b, f"{kind}_out").signal(f"{kind}_last")
            getattr(a, f"{kind}_in").bind(first)
            getattr(a, f"{kind}_out").bind(middle)
            getattr(b, f"{kind}_in").bind(middle)
            getattr(b, f"{kind}_out").bind(last)
            buses[kind] = (first, last)
        # A port only binds to a signal of its struct.
        with pytest.raises(ValueError, match="another value type"):
            b.p_in.bind(buses["u"][0])
        a.clk.bind(clk)
        b.clk.bind(clk)
        context.elaborate()

        (u_first, u_last), (p_first, p_last), (w_first, w_last) = buses.values()
        u_first.write((-321, 0, [1, 2, 3, 4], (11, 0), to_words(1 << 99, 4), (0, 0)))
        p_first.write((654, 0, (1, 0)))
        w_first.write((to_words(0, 4), -(1 << 38), (0, 0), 5))
        context.run(30)

        # Two stages: negated twice, reversed twice, swapped twice, inverted twice.
        u = u_last.read()
        assert u.data == -321
        assert u.taps.tolist() == [1, 2, 3, 4]
        assert u.iq.re == 11
        assert from_words(u.wide) == 1 << 99
        assert u.flags.id == 2
        assert (p_last.read().data, p_last.read().flags.id) == (654, 5)
        assert (w_last.read().mid, w_last.read().lo) == (-(1 << 38), 7)


def test_value_struct_arrays():
    """An array of structs is an array of ports, read and written as one numpy array."""

    @vbuilder(source=HDL_DIR / "StructArrayModel.sv", include_dirs=[HDL_DIR])
    class ValueArrays(VModule):
        clk: InputU8
        a_in: PortBaseArrayView
        a_out: PortBaseArrayView
        f_in: PortBaseArrayView
        f_out: PortBaseArrayView
        w_in: PortBaseArrayView
        w_out: PortBaseArrayView
        o_in: PortBase
        o_out: PortBase
        u_in: InputU16
        u_out: OutputU16
        pk_in: InputU16
        pk_out: OutputU16

    with Context() as context:
        clk = Clock("clk", 10)
        dut = ValueArrays("dut")

        assert isinstance(dut.a_in, PortBaseArrayView)
        assert (dut.a_in.shape, dut.a_in.size, len(dut.a_in)) == ((2, 3), 6, 6)
        assert dut.a_in.dtype.names == ("data", "taps", "iq", "wide", "flags")
        element = dut.a_in[1, 2]
        assert type(element) is PortBase
        assert element.name == "a_in[1][2]"
        assert dut.a_in[1].shape == (3,)
        assert dut.a_in[:, 1:].shape == (2, 2)
        assert [p.name for p in dut.f_in] == [f"f_in[{i}]" for i in range(4)]
        assert dut.f_in.dtype == FLAGS
        # Arrays of structs that are fields of a struct are arrays in its value.
        assert dut.o_in.dtype.names == ("iqs", "flags", "valid")
        assert dut.o_in.dtype["iqs"] == np.dtype((IQ, (3,)))
        assert dut.o_in.dtype["flags"] == np.dtype((FLAGS, (2,)))
        # A packed union and a packed array of structs are vectors, as before.
        assert (dut.u_in.width, dut.pk_in.width) == (16, 12)

        s = connect(dut, [f"{n}_{d}" for n in "afwo" for d in ("in", "out")])
        u_in, u_out = SignalU16("u_in"), SignalU16("u_out")
        pk_in, pk_out = SignalU16("pk_in", 12), SignalU16("pk_out", 12)
        dut.clk.bind(clk)
        dut.u_in.bind(u_in)
        dut.u_out.bind(u_out)
        dut.pk_in.bind(pk_in)
        dut.pk_out.bind(pk_out)
        context.elaborate()
        assert isinstance(s["a_in"], SignalBaseArrayView)
        assert s["a_in"].shape == (2, 3)
        assert s["a_in"][1, 2].name == "a_in[1][2]"

        rng = random.Random(3)
        index = [(i, j) for i in range(2) for j in range(3)]
        for _ in range(50):
            # 2 x 3 unpacked structs, as one array.
            a = np.zeros((2, 3), dut.a_in.dtype)
            a["data"] = [
                [rng.randrange(-1000, 1000) for _ in range(3)] for _ in range(2)
            ]
            a["taps"] = [
                [[rng.randrange(64), rng.randrange(64)] for _ in range(3)]
                for _ in range(2)
            ]
            a["iq"]["re"] = [
                [rng.randrange(-127, 128) for _ in range(3)] for _ in range(2)
            ]
            a["iq"]["im"] = [
                [rng.randrange(-100, 100) for _ in range(3)] for _ in range(2)
            ]
            wide = {ij: rng.getrandbits(100) for ij in index}
            for ij in index:
                a["wide"][ij] = to_words(wide[ij], 4)
            a["flags"]["id"] = [[rng.randrange(8) for _ in range(3)] for _ in range(2)]
            a["flags"]["last"] = [
                [rng.randrange(2) for _ in range(3)] for _ in range(2)
            ]
            s["a_in"].write(a)
            # 4 packed structs, as a list of tuples.
            f_id = [rng.randrange(8) for _ in range(4)]
            f_last = [rng.randrange(2) for _ in range(4)]
            s["f_in"].write(list(zip(f_id, f_last, strict=True)))
            # 2 packed structs of 86 bits, element by element.
            w_wide = [rng.getrandbits(70) for _ in range(2)]
            w_data = [rng.randrange(-1000, 1000) for _ in range(2)]
            w_id = [rng.randrange(8) for _ in range(2)]
            for i in range(2):
                s["w_in"][i].write(
                    (to_words(w_wide[i], 3), w_data[i], (w_id[i], 1 - i))
                )
            # Arrays of structs in a struct.
            o_re = [rng.randrange(-100, 100) for _ in range(3)]
            o_im = [rng.randrange(-100, 100) for _ in range(3)]
            o_id = [rng.randrange(8) for _ in range(2)]
            s["o_in"].write(
                {
                    "iqs": list(zip(o_re, o_im, strict=True)),
                    "flags": [(o_id[0], 0), (o_id[1], 1)],
                    "valid": 1,
                }
            )
            imm, low = rng.randrange(1, 2048), rng.getrandbits(12)
            u_in.d = (rng.randrange(16) << 12) | imm
            pk_in.d = low
            context.run(10)

            out = s["a_out"].read()
            assert out.shape == (2, 3)
            for i, j in index:
                element = out[i, j]
                assert element.data == a["data"][i, j] + 3 * i + j
                assert element.taps.tolist() == a["taps"][i, j].tolist()[::-1]
                assert element.iq.re == -a["iq"]["re"][i, j]
                assert element.iq.im == a["iq"]["im"][i, j] + i
                assert from_words(element.wide) == wide[i, j] ^ ((1 << 100) - 1)
                assert element.flags.id == (a["flags"]["id"][i, j] + j) % 8
                assert element.flags.last == 1 - a["flags"]["last"][i, j]
            # The same, field by field, and through the ports.
            expected = a["data"] + np.array([[0, 1, 2], [3, 4, 5]])
            assert (out["data"] == expected).all()
            assert (dut.a_out.read()["iq"]["re"] == -a["iq"]["re"]).all()
            assert s["a_out"][1, 2].read().data == expected[1, 2]

            f = s["f_out"].read()
            assert f["id"].tolist() == [(x + i) % 8 for i, x in enumerate(f_id)]
            assert f["last"].tolist() == f_last
            w = s["w_out"].read()
            for i in range(2):
                assert from_words(w[i].wide) == w_wide[i] ^ ((1 << 70) - 1)
            assert w["data"].tolist() == [w_data[0], w_data[1] - 1]
            assert w["flags"]["id"].tolist() == [(w_id[0] + 1) % 8, (w_id[1] + 2) % 8]
            assert w["flags"]["last"].tolist() == [1, 0]
            o = s["o_out"].read()
            assert o.iqs["re"].tolist() == o_im
            assert o.iqs["im"].tolist() == [x + i for i, x in enumerate(o_re)]
            assert o.flags["id"].tolist() == o_id[::-1]
            assert o.flags["last"].tolist() == [1, 0]
            assert o.valid == 1
            assert u_out.q == ((imm & 0xF) << 12) | (-imm & 0xFFF)
            assert pk_out.q == ((low & 0xF) << 8) | (low & 0xF0) | (low >> 8)


class DataMonitor(Module):
    """A Python module with a port for one field of a bus."""

    def __init__(self, name: str):
        self.data = InputS16("data")
        self.seen: list[int] = []
        self.add_task(self.run())

    async def run(self):
        while True:
            await self.wait(self.data.change())
            self.seen.append(self.data.read())


def test_value_struct_fields():
    """The fields of the struct signal of a verilated module are signals for ports of their types."""
    StructValue = build_vmodule(
        "StructValue", source=HDL_DIR / "StructModel.sv", include_dirs=[HDL_DIR]
    )
    with Context() as context:
        clk = Clock("clk", 10)
        dut = StructValue("dut")
        s = connect(dut, STRUCT_PORTS)
        dut.clk.bind(clk)
        data = s["u_out"]["data"]
        assert isinstance(data, FieldSignalS16)
        assert data.name == "u_out.data"
        monitor = DataMonitor("monitor")
        monitor.data.bind(data)
        # A field of the input is driven on its own.
        valid = s["u_in"]["valid"]
        context.elaborate()

        s["u_in"].write((5, 0, [0] * 4, (0, 0), [0] * 4, (0, 0)))
        context.run(10)
        assert monitor.seen == [-5]
        # Another field changes: the monitor is not woken.
        valid.write(1)
        context.run(10)
        assert s["u_out"].read().valid == 1
        assert s["u_out"].read().data == -5
        assert monitor.seen == [-5]
        value = s["u_in"].read()
        value.data = 6
        s["u_in"].write(value)
        context.run(10)
        assert monitor.seen == [-5, -6]


def test_value_struct_annotations():
    """A struct port is annotated as a PortBase, or as a PortStruct if the module has a port per field."""
    source = HDL_DIR / "StructModel.sv"

    with pytest.raises(
        TypeError,
        match=r"expected a struct \(PortBase\), got a struct \(PortStruct\)",
    ) as error:

        @vbuilder(source=source, include_dirs=[HDL_DIR])
        class ValueAsField(VModule):
            clk: InputU8
            u_in: PortStruct
            u_out: PortBase
            p_in: PortBase
            p_out: PortBase
            w_in: PortBase
            w_out: PortBase

    assert 'struct_ports="fields"' in str(error.value)

    with pytest.raises(
        TypeError,
        match=r"expected a struct \(PortStruct\), got a struct \(PortBase\)",
    ):

        @vbuilder(source=source, include_dirs=[HDL_DIR], struct_ports="fields")
        class FieldAsValue(VModule):
            clk: InputU8
            u_in: PortBase
            u_out: PortStruct
            p_in: PortStruct
            p_out: PortStruct
            w_in: PortStruct
            w_out: PortStruct

    with pytest.raises(
        TypeError,
        match=r"expected a struct \(PortBase\), got an array of structs",
    ):

        @vbuilder(source=source, include_dirs=[HDL_DIR])
        class ValueAsArray(VModule):
            clk: InputU8
            u_in: PortBaseArrayView
            u_out: PortBase
            p_in: PortBase
            p_out: PortBase
            w_in: PortBase
            w_out: PortBase

    with pytest.raises(TypeError, match="Struct mismatch for port 'u_in'"):

        @vbuilder(source=source, include_dirs=[HDL_DIR])
        class ValueAsPort(VModule):
            clk: InputU8
            u_in: InputU8
            u_out: PortBase
            p_in: PortBase
            p_out: PortBase
            w_in: PortBase
            w_out: PortBase
