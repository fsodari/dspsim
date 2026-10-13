"""
Signals and ports whose value is a struct, an array or another type without classes of its own in Python.
A struct is a RecordSignal or a RecordPort: its fields are attributes, each a signal that is read and
written on its own, and the whole value is a numpy record. Any other type is a SignalBase or a PortBase.
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
    RecordPort,
    RecordSignal,
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
    bus_in: RecordPort
    bus_out: RecordPort
    sum: RecordPort
    frame_in: PortBase
    frame_out: PortBase
    samples: PortBaseArrayView
    conjugates: PortBaseArrayView
    scale: PortBase
    flag: PortBase
    named_in: RecordPort
    named_out: RecordPort
    # A RecordPort is a PortBase.
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
    *(
        "scale",
        "flag",
        "named_in",
        "named_out",
        "opaque_in",
        "opaque_out",
        "evaluations",
    ),
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
    """A port of a type without framework classes is a PortBase in Python (a RecordPort, for a struct)."""
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
        assert type(stage.bus_in) is RecordPort
        assert isinstance(stage.bus_in, PortBase)
        assert stage.bus_in.dtype == BUS
        assert stage.sum.dtype == IQ
        assert not stage.bus_in.is_output and stage.bus_out.is_output

        s = connect(stage)
        bus_in, bus_out = s["bus_in"], s["bus_out"]
        assert type(bus_in) is RecordSignal
        assert isinstance(bus_in, SignalBase)
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
        # Not structs: a PortBase and a SignalBase.
        assert type(stage.frame_in) is PortBase
        assert type(s["frame_in"]) is SignalBase
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
        assert type(stage.samples[1, 2]) is RecordPort
        assert all(type(port) is RecordPort for port in stage.samples)
        assert type(stage.samples.flat(5)) is RecordPort
        assert stage.samples[1, 2].name == "samples[1][2]"
        assert stage.samples[1].shape == (3,)

        s = connect(stage)
        samples, conjugates = s["samples"], s["conjugates"]
        assert isinstance(samples, SignalBaseArrayView)
        assert samples.shape == (2, 3)
        assert samples[0, 1].name == "samples[0][1]"
        assert type(samples[0, 1]) is RecordSignal
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
    """The fields of a struct signal are attributes: signals that are read and written on their own."""
    with Context() as context:
        stage = ValueStage("stage")
        s = connect(stage)
        bus_in, bus_out = s["bus_in"], s["bus_out"]
        context.elaborate()
        s["scale"].write(1.0)

        # Each field is written on its own, and the others keep their value. Nothing had to be asked for
        # before the design was elaborated.
        bus_in.tag.d = 7
        bus_in.data.write(100)
        bus_in.iq.re.d = 3
        bus_in.iq.im.d = -4
        bus_in.taps.d = [1, 2, 3]
        bus_in.gain.d = 1.5
        context.run(1)
        assert plain(bus_in.q) == (7, False, 100, (3, -4), [1, 2, 3], 1.5)
        bus_in.valid.d = True
        bus_in.taps[0].d = 9
        context.run(1)
        assert plain(bus_in.q) == (7, True, 100, (3, -4), [9, 2, 3], 1.5)

        # And read on its own.
        assert bus_out.tag.q == 8
        assert bus_out.data.read() == 200
        assert (bus_out.iq.re.q, bus_out.iq.im.q) == (-4, 3)
        assert bus_out.taps.q.tolist() == [3, 2, 9]
        assert bus_out.taps[2].q == 9
        assert bus_out.gain.value == 1.5
        assert bus_out.valid.q is False
        # A nested struct as a whole.
        assert plain(bus_out.iq.q) == (-4, 3)
        bus_in.iq.d = (10, 20)
        context.run(1)
        assert plain(bus_out.iq.read()) == (20, 10)
        # What was written last, before it is committed.
        bus_in.tag.d = 50
        assert bus_in.tag.d == 50
        assert bus_in.tag.q == 7
        assert bus_in.d.tag == 50
        context.run(1)

        # A field is a signal of the class of its type, and the same object every time.
        tag = bus_out.tag
        assert isinstance(tag, FieldSignalU16) and isinstance(tag, SignalU16)
        assert tag.name == "bus_out.tag"
        assert tag.source is bus_out
        assert bus_out.tag is tag
        assert bus_out["tag"] is tag
        assert isinstance(bus_in.data, FieldSignalU32)
        assert isinstance(bus_in.gain, FieldSignalFloat)
        # A nested struct has fields of its own, an array is an array of signals.
        assert type(bus_in.iq) is RecordSignal
        assert bus_in.iq.dtype == IQ
        assert bus_in.iq.fields == ["re", "im"]
        assert isinstance(bus_in.iq.re, FieldSignalS16) and isinstance(
            bus_in.iq.re, SignalS16
        )
        assert bus_in.iq.re.name == "bus_in.iq.re"
        assert isinstance(bus_out.taps, SignalBaseArrayView)
        assert bus_out.taps.shape == (3,)
        assert isinstance(bus_out.taps[2], FieldSignalU8)
        # A bool has no class of its own.
        assert type(bus_in.valid) is SignalBase
        assert "tag" in bus_in and "nothing" not in bus_in

        with pytest.raises(AttributeError, match="has no attribute 'nothing'"):
            _ = bus_in.nothing
        with pytest.raises(KeyError, match="no field 'nothing'"):
            bus_in["nothing"]
        with pytest.raises(TypeError):
            tag["x"]

        # The fields of a port are those of its signal.
        assert stage.bus_out.tag is tag
        assert stage.bus_out.iq.re.q == 20
        assert stage.bus_out.fields == bus_out.fields
        assert stage.bus_out["data"] is bus_out.data


def test_struct_field_events_and_binding():
    """A field has its own events, and ports of the field's type bind to it."""
    with Context() as context:
        stage = ValueStage("stage")
        s = connect(stage)
        bus_in, bus_out = s["bus_in"], s["bus_out"]

        # A module with a port per field reads one field of a bus and drives two of another.
        fields = FieldModule("fields")
        fields.tag.bind(bus_out.tag)
        fields.re.bind(bus_in.iq.re)
        fields.data.bind(bus_in.data)
        context.elaborate()

        bus_in.write((5, False, 1, (2, 3), [4, 5, 6], 1.0))
        context.run(1)
        # The stage adds one to the tag; the module writes it back to two fields of the input.
        assert bus_out.tag.q == 6
        assert fields.seen[-1] == 6
        assert plain(bus_in.read()) == (5, False, 1006, (-6, 3), [4, 5, 6], 1.0)
        assert bus_out.data.q == 2012

        # The events of a field only occur when the field changes.
        seen = len(fields.seen)
        bus_in.gain.d = 2.0
        bus_in.taps.d = [0, 0, 0]
        context.run(1)
        assert len(fields.seen) == seen
        bus_in.tag.d = 6
        context.run(1)
        assert fields.seen[seen:] == [7]


class LateWatcher(Module):
    """Waits for a field that it only asks for while the simulation runs."""

    def __init__(self, name: str, bus):
        self.bus = bus
        self.seen: list[int] = []
        self.add_task(self.run())

    async def run(self):
        while True:
            await self.wait(self.bus.data.change())
            self.seen.append(self.bus.data.q)


def test_struct_field_created_while_running():
    """A process can wait for a field that nobody asked for before the design was elaborated."""
    with Context() as context:
        stage = ValueStage("stage")
        s = connect(stage)
        watcher = LateWatcher("watcher", s["bus_out"])
        context.elaborate()
        context.run(1)

        s["bus_in"].data.d = 4
        context.run(1)
        s["bus_in"].tag.d = 1
        context.run(1)
        s["bus_in"].data.d = 5
        context.run(1)
        assert watcher.seen == [8, 10]


def test_struct_field_names():
    """A field comes before the attributes that every signal has, except those a struct signal is used with."""
    with Context() as context:
        stage = ValueStage("stage")
        s = connect(stage)
        named_in, named_out = s["named_in"], s["named_out"]
        context.elaborate()

        # `id` and `width` are fields, not the id of the model and the width of the signal.
        named_in.id.d = 5
        named_in.width.d = 600
        context.run(1)
        assert (named_out.id.q, named_out.width.q) == (6, 601)
        assert named_in.id.name == "named_in.id"
        # `value` and `name` are the value and the name of the struct signal: those fields are items.
        assert named_out.name == "named_out"
        assert plain(named_out.value) == (6, 601, 1, 1)
        named_in["value"].d = -3
        named_in["name"].d = 9
        context.run(1)
        assert (named_out["value"].q, named_out["name"].q) == (-2, 10)
        assert named_out.fields == ["id", "width", "value", "name"]
        # The same on the port.
        assert stage.named_out.id.q == 6
        assert stage.named_out.name == "named_out"
        assert stage.named_out["name"].q == 10
