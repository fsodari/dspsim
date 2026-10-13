"""
Signals and ports whose value is a struct, an array or another type without classes of its own in Python:
they are a SignalBase or PortBase, read and written as numpy values, and the fields of a struct signal are
signals.
"""

from pathlib import Path

import numpy as np
import pytest

from dspsim.builder import CppModule, cppbuilder
from dspsim.cpp_parser import load_cpp_module_info
from dspsim.framework import (
    Context,
    FieldSignalFloat,
    FieldSignalS16,
    FieldSignalU8,
    FieldSignalU16,
    FieldSignalU32,
    InputU16,
    Module,
    OutputS16,
    OutputU32,
    PortBase,
    PortBaseArrayView,
    Record,
    SignalBase,
    SignalBaseArrayView,
    SignalS16,
    SignalU16,
    SignalU32,
)

CPP_DIR = Path(__file__).parent / "cpp_modules"

IQ = np.dtype([("re", "<i2"), ("im", "<i2")])
# The offsets are those of the C++ struct, with its padding.
BUS = np.dtype(
    {
        "names": ["tag", "valid", "data", "iq", "taps", "gain"],
        "formats": ["<u2", "?", "<u4", IQ, ("u1", (3,)), "<f8"],
        "offsets": [0, 2, 4, 8, 12, 16],
        "itemsize": 24,
    }
)


@cppbuilder(header=CPP_DIR / "ValuePorts.h", cls="demo::ValueStage")
class ValueStage(CppModule):
    bus_in: PortBase
    bus_out: PortBase
    sum: PortBase
    frame_in: PortBase
    frame_out: PortBase
    samples: PortBaseArrayView
    conjugates: PortBaseArrayView
    scale: PortBase
    flag: PortBase
    opaque_in: PortBase
    opaque_out: PortBase
    evaluations: OutputU32


def plain(value):
    """A numpy value as nested tuples (structs), lists (arrays) and Python numbers."""
    if isinstance(value, np.ndarray):
        return [plain(item) for item in value]
    if isinstance(value, np.void):
        return tuple(plain(value[name]) for name in value.dtype.names)
    return value.item() if isinstance(value, np.generic) else value


PORTS = [
    *("bus_in", "bus_out", "sum", "frame_in", "frame_out", "samples", "conjugates"),
    *("scale", "flag", "opaque_in", "opaque_out", "evaluations"),
]


def connect(stage) -> dict[str, SignalBase | SignalBaseArrayView]:
    """Bind every port of a stage to a signal that it creates."""
    signals = {}
    for name in PORTS:
        port = getattr(stage, name)
        if isinstance(port, PortBaseArrayView):
            signals[name] = port.signals(name)
        else:
            signals[name] = port.signal(name)
        port.bind(signals[name])
    return signals


def test_cpp_value_port_info():
    """A port of a type without framework classes is a PortBase in Python."""
    info = load_cpp_module_info(CPP_DIR / "ValuePorts.h", "demo::ValueStage")
    types = {name: port.framework_type for name, port in info.ports.items()}
    assert types["bus_in"] == "PortBase"
    assert types["samples"] == "PortBaseArrayView"
    assert types["scale"] == "PortBase"
    assert types["evaluations"] == "OutputU32"
    assert info.ports["bus_in"].ctype == "demo::Bus"
    assert info.ports["bus_in"].direction == "input"
    assert not info.ports["bus_in"].is_struct


def test_struct_port():
    """A struct is read as a numpy record and written from a record, a tuple or a mapping."""
    with Context() as context:
        stage = ValueStage("stage")
        assert type(stage.bus_in) is PortBase
        assert stage.bus_in.dtype == BUS
        assert stage.sum.dtype == IQ
        assert not stage.bus_in.is_output and stage.bus_out.is_output

        s = connect(stage)
        bus_in, bus_out = s["bus_in"], s["bus_out"]
        assert type(bus_in) is SignalBase
        assert bus_in.name == "bus_in"
        assert bus_in.dtype == BUS
        assert bus_in.fields == ["tag", "valid", "data", "iq", "taps", "gain"]
        context.elaborate()

        s["scale"].write(2.0)
        bus_in.write((7, True, 100, (3, -4), [1, 2, 3], 1.5))
        context.run(1)

        value = bus_out.read()
        # A Record is a numpy.record whose fields come first: `data` is the field, not the buffer of numpy.
        assert isinstance(value, Record) and isinstance(value, np.record)
        assert value.dtype == BUS
        assert (value.tag, value.valid, value.data) == (8, False, 200)
        assert (value.iq.re, value.iq.im) == (-4, 3)
        assert list(value.taps) == [3, 2, 1]
        assert value.gain == 3.0
        assert value["data"] == 200
        # The port reads the same value, also as `value` and `q`.
        assert stage.bus_out.read() == value
        assert stage.bus_out.value == value
        assert bus_out.q == value
        assert (s["sum"].read().re, s["sum"].read().im) == (-1, 7)

        # What is read is a copy: change it and write it back.
        value = bus_in.read()
        value.tag = 20
        value.iq.im = 50
        bus_in.write(value)
        context.run(1)
        assert bus_out.read().tag == 21
        assert bus_out.read().iq.re == 50
        assert bus_out.read().data == 200

        # A mapping names every field.
        bus_in.value = {
            "tag": 1,
            "valid": False,
            "data": 5,
            "iq": {"re": 1, "im": 2},
            "taps": [9, 8, 7],
            "gain": 0.25,
        }
        context.run(1)
        assert plain(bus_out.read()) == (2, True, 10, (2, 1), [7, 8, 9], 0.5)

        # An output port writes its signal.
        stage.sum.write((11, 12))
        context.run(1)
        assert plain(s["sum"].read()) == (11, 12)

        with pytest.raises(ValueError, match="no field 'valid'"):
            bus_in.write({"tag": 1})
        with pytest.raises(ValueError, match="no field 'extra'"):
            s["sum"].write({"re": 1, "im": 2, "extra": 3})
        with pytest.raises(ValueError):
            bus_in.write((1, 2))
        with pytest.raises(RuntimeError, match="is an input"):
            stage.bus_in.write(value)


def test_unchanged_struct_is_no_change():
    """Writing the value that a struct signal has does not evaluate the processes that read it."""
    with Context() as context:
        stage = ValueStage("stage")
        s = connect(stage)
        context.elaborate()
        context.run(1)
        count = s["evaluations"].q

        s["bus_in"].write((1, False, 2, (3, 4), [5, 6, 7], 8.0))
        context.run(1)
        assert s["evaluations"].q == count + 1
        s["bus_in"].write(s["bus_in"].read())
        context.run(1)
        assert s["evaluations"].q == count + 1


def test_other_value_types():
    """An array as the value of a port, and numbers that have no classes of their own."""
    with Context() as context:
        stage = ValueStage("stage")
        s = connect(stage)
        assert stage.frame_in.dtype == np.dtype(("<i2", (4,)))
        assert stage.scale.dtype == np.dtype("<f4")
        assert stage.flag.dtype == np.dtype("?")
        context.elaborate()

        s["frame_in"].write([1, -2, 3, -4])
        s["scale"].write(1.5)
        context.run(1)
        frame = s["frame_out"].read()
        assert isinstance(frame, np.ndarray)
        assert frame.tolist() == [-1, 2, -3, 4]
        assert s["flag"].read() is True
        assert s["scale"].read() == 1.5
        # One value for every element.
        s["frame_in"].write(7)
        context.run(1)
        assert stage.frame_out.read().tolist() == [-7] * 4

        with pytest.raises(ValueError):
            s["frame_in"].write([1, 2, 3])


def test_struct_port_array():
    """An array of ports with a struct as their value is read and written as one numpy array."""
    with Context() as context:
        stage = ValueStage("stage")
        assert isinstance(stage.samples, PortBaseArrayView)
        assert stage.samples.shape == (2, 3)
        assert stage.samples.dtype == IQ
        assert type(stage.samples[1, 2]) is PortBase
        assert stage.samples[1, 2].name == "samples[1][2]"
        assert stage.samples[1].shape == (3,)

        s = connect(stage)
        samples, conjugates = s["samples"], s["conjugates"]
        assert isinstance(samples, SignalBaseArrayView)
        assert samples.shape == (2, 3)
        assert samples[0, 1].name == "samples[0][1]"
        context.elaborate()

        values = np.zeros((2, 3), IQ)
        values["re"] = np.arange(6).reshape(2, 3)
        values["im"] = 10
        samples.write(values)
        context.run(1)
        result = conjugates.read()
        # A numpy array: its fields are items, and its elements are Records.
        assert isinstance(result, np.ndarray)
        assert result.shape == (2, 3)
        assert result.dtype == IQ
        assert result["re"].tolist() == [[0, 1, 2], [3, 4, 5]]
        assert (result["im"] == -10).all()
        assert isinstance(result[1, 2], Record)
        assert result[1, 2].re == 5
        assert np.asarray(conjugates).shape == (2, 3)
        assert stage.conjugates.read()["im"].tolist() == result["im"].tolist()

        # An element, a view, and one value for all.
        samples[1, 2].write((7, 8))
        samples[0].write((1, 1))
        context.run(1)
        assert plain(conjugates[1, 2].read()) == (7, -8)
        assert conjugates[0].read()["im"].tolist() == [-1, -1, -1]
        samples.write((2, 3))
        context.run(1)
        assert (conjugates.value["im"] == -3).all()

        with pytest.raises(ValueError):
            samples.write(np.zeros((3, 3), IQ))


def test_value_port_binding():
    """A port binds to a signal or a port of its value type, whatever the type is."""
    with Context():
        stage = ValueStage("stage")
        other = ValueStage("other")
        bus = stage.bus_out.signal("bus")
        stage.bus_out.bind(bus)
        other.bus_in(bus)

        with pytest.raises(ValueError, match="another value type"):
            other.sum.bind(bus)
        with pytest.raises(ValueError, match="another value type"):
            other.bus_out.bind(SignalU32("number"))
        with pytest.raises(ValueError, match="an input binds to an input"):
            other.bus_out.bind(stage.bus_in)
        with pytest.raises(ValueError, match="same shape"):
            other.samples.bind(stage.conjugates.signals("x")[0])
        # A port of a number creates the signal of its class.
        assert isinstance(stage.evaluations.signal("n"), SignalU32)


def test_opaque_struct():
    """A struct without a DSPSIM_STRUCT works in C++, and Python can bind its ports but not read them."""
    with Context() as context:
        stage = ValueStage("stage")
        s = connect(stage)
        context.elaborate()
        context.run(1)
        with pytest.raises(TypeError, match="without a DSPSIM_STRUCT"):
            s["opaque_out"].read()
        with pytest.raises(TypeError, match="without a DSPSIM_STRUCT"):
            _ = stage.opaque_in.dtype
        assert s["opaque_out"].fields == []


class FieldModule(Module):
    """A Python module with a port per field of the bus."""

    def __init__(self, name: str):
        self.tag = InputU16("tag")
        self.re = OutputS16("re")
        self.data = OutputU32("data")
        self.seen: list[int] = []
        self.add_task(self.run())

    async def run(self):
        while True:
            await self.wait(self.tag.change())
            self.seen.append(self.tag.read())
            self.re.write(-self.tag.read())
            self.data.write(1000 + self.tag.read())


def test_struct_fields():
    """A field of a struct signal is a signal: ports of the field's type bind to it."""
    with Context() as context:
        stage = ValueStage("stage")
        s = connect(stage)
        bus_in, bus_out = s["bus_in"], s["bus_out"]

        tag = bus_out["tag"]
        assert isinstance(tag, FieldSignalU16) and isinstance(tag, SignalU16)
        assert tag.name == "bus_out.tag"
        assert tag.source is bus_out
        # Asked for again, it is the same signal.
        assert bus_out["tag"] is tag
        assert isinstance(bus_in["data"], FieldSignalU32)
        assert isinstance(bus_in["gain"], FieldSignalFloat)
        # A nested struct is a signal with fields of its own.
        iq = bus_in["iq"]
        assert type(iq) is SignalBase
        assert iq.dtype == IQ
        assert isinstance(iq["re"], FieldSignalS16) and isinstance(iq["re"], SignalS16)
        assert iq["re"].name == "bus_in.iq.re"
        # A bool has no class of its own.
        assert type(bus_in["valid"]) is SignalBase
        # The elements of an array are signals.
        taps = bus_out["taps"]
        assert isinstance(taps, SignalBaseArrayView)
        assert taps.shape == (3,)
        assert isinstance(taps[2], FieldSignalU8)
        with pytest.raises(KeyError, match="no field 'nothing'"):
            bus_in["nothing"]
        with pytest.raises(TypeError, match="has no fields"):
            tag["x"]

        # A module with a port per field reads one field of a bus and drives two of another.
        fields = FieldModule("fields")
        fields.tag.bind(tag)
        fields.re.bind(iq["re"])
        fields.data.bind(bus_in["data"])
        context.elaborate()

        bus_in.write((5, False, 1, (2, 3), [4, 5, 6], 1.0))
        context.run(1)
        # The stage adds one to the tag; the module writes it back to two fields of the input.
        assert tag.read() == 6
        assert fields.seen[-1] == 6
        assert plain(bus_in.read()) == (5, False, 1006, (-6, 3), [4, 5, 6], 1.0)
        assert bus_out.read().data == 2012
        assert taps.read().tolist() == [6, 5, 4]
        assert taps[0].read() == 6

        # The events of a field only occur when the field changes.
        seen = len(fields.seen)
        value = bus_in.read()
        value.gain = 2.0
        value.taps = [0, 0, 0]
        bus_in.write(value)
        context.run(1)
        assert len(fields.seen) == seen
        assert plain(iq.read()) == (-6, 3)

        # Fields are written through to the struct.
        bus_in["valid"].write(True)
        iq.write((9, 9))
        context.run(1)
        assert bus_in.read().valid
        assert plain(bus_in.read().iq) == (9, 9)

        # A new field cannot be made once the design is elaborated; one that exists is still there.
        assert bus_out["tag"] is tag
        with pytest.raises(RuntimeError, match="before the design is elaborated"):
            bus_out["data"]
