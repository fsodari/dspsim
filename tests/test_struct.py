"""SignalStruct and PortStruct: groups of signals and ports with named fields, bound as a whole."""

import subprocess
import sys

import pytest

from dspsim.framework import (
    Context,
    InputArrayU8,
    InputArrayViewS16,
    InputS8,
    InputS16,
    InputU8,
    Module,
    OutputArrayU8,
    OutputS8,
    OutputS16,
    OutputU8,
    PortStruct,
    PortStructArray,
    SignalArrayU8,
    SignalArrayViewS8,
    SignalS8,
    SignalS16,
    SignalS32,
    SignalStruct,
    SignalStructArray,
    SignalU8,
)


def bus_ports(name: str, scalar16, scalar8, signed8, array8) -> PortStruct:
    """The ports of a bus with a nested struct and an array, as inputs or as outputs."""
    iq = PortStruct(f"{name}.iq")
    iq.add("re", signed8(f"{name}.iq.re")).add("im", signed8(f"{name}.iq.im"))
    ports = PortStruct(name)
    ports.add("data", scalar16(f"{name}.data", 12))
    ports.add("valid", scalar8(f"{name}.valid", 1))
    ports.add("taps", array8(f"{name}.taps", (4,), 6))
    ports.add("iq", iq)
    return ports


class Stage(Module):
    """Negates the data and the sample, reverses the taps."""

    def __init__(self, name: str):
        self.bus_in = bus_ports("bus_in", InputS16, InputU8, InputS8, InputArrayU8)
        self.bus_out = bus_ports(
            "bus_out", OutputS16, OutputU8, OutputS8, OutputArrayU8
        )
        self.process(self.eval, "Stage.eval").always("*")

    def eval(self):
        self.bus_out.data.d = -self.bus_in.data.q
        self.bus_out.valid.d = self.bus_in.valid.q
        for i in range(4):
            self.bus_out.taps[i].d = self.bus_in.taps[3 - i].q
        self.bus_out.iq.re.d = -self.bus_in.iq.re.q
        self.bus_out.iq.im.d = -self.bus_in.iq.im.q


class Wrapper(Module):
    """A stage inside a module with struct ports of its own."""

    def __init__(self, name: str):
        self.bus_in = bus_ports("bus_in", InputS16, InputU8, InputS8, InputArrayU8)
        self.bus_out = bus_ports(
            "bus_out", OutputS16, OutputU8, OutputS8, OutputArrayU8
        )
        self.stage = Stage("stage")
        self.stage.bus_in.bind(self.bus_in)
        self.stage.bus_out.bind(self.bus_out)


def test_port_struct_fields():
    with Context():
        stage = Stage("stage")
        ports = stage.bus_in
        assert ports.name == "bus_in"
        assert ports.fields == ["data", "valid", "taps", "iq"]
        assert "data" in ports and "ready" not in ports

        # A field is an attribute and an item, with the type of its port.
        assert isinstance(ports.data, InputS16)
        assert ports.data is ports["data"]
        assert ports.data.width == 12
        assert isinstance(ports.taps, InputArrayU8)
        assert ports.taps.shape == (4,)
        assert isinstance(ports.iq, PortStruct)
        assert ports.iq.fields == ["re", "im"]
        assert isinstance(stage.bus_out.iq.re, OutputS8)

        with pytest.raises(AttributeError, match="has no field 'ready'"):
            _ = ports.ready
        with pytest.raises(KeyError):
            ports["ready"]
        with pytest.raises(ValueError, match="already has a field 'data'"):
            ports.add("data", InputS16("again"))
        with pytest.raises(TypeError, match="A field of a PortStruct"):
            ports.add("signal", SignalU8("signal"))


def test_signals_of_port_struct():
    with Context():
        stage = Stage("stage")
        bus = stage.bus_in.signals("bus")
        assert isinstance(bus, SignalStruct)
        assert bus.name == "bus"
        assert bus.fields == ["data", "valid", "taps", "iq"]

        # One signal per field, with the type, width and shape of the port.
        assert isinstance(bus.data, SignalS16)
        assert bus.data.name == "bus.data"
        assert bus.data.width == 12
        assert bus.valid.width == 1
        assert isinstance(bus.taps, SignalArrayU8)
        assert bus.taps.shape == (4,)
        assert bus.taps[0].width == 6
        assert isinstance(bus.iq, SignalStruct)
        assert bus.iq.re.name == "bus.iq.re"
        assert bus.data is bus["data"]


def test_whole_struct_bind():
    with Context() as ctx:
        a = Stage("a")
        b = Stage("b")
        bus_in = a.bus_in.signals("bus_in")
        mid = a.bus_out.signals("mid")
        bus_out = b.bus_out.signals("bus_out")
        a.bus_in.bind(bus_in)
        a.bus_out.bind(mid)
        b.bus_in.bind(mid)
        b.bus_out.bind(bus_out)
        ctx.elaborate()

        bus_in.data.d = -123
        bus_in.valid.d = 1
        bus_in.taps.d = [10, 11, 12, 13]
        bus_in.iq.re.d = 5
        bus_in.iq.im.d = -6
        ctx.run(1)

        assert mid.data.q == 123
        assert list(mid.taps.q) == [13, 12, 11, 10]
        assert mid.iq.im.q == 6
        # Two stages undo each other.
        assert bus_out.data.q == -123
        assert bus_out.valid.q == 1
        assert list(bus_out.taps.q) == [10, 11, 12, 13]
        assert (bus_out.iq.re.q, bus_out.iq.im.q) == (5, -6)


def test_struct_bind_to_parent():
    with Context() as ctx:
        top = Wrapper("top")
        bus_in = top.bus_in.signals("bus_in")
        bus_out = top.bus_out.signals("bus_out")
        top.bus_in.bind(bus_in)
        top.bus_out.bind(bus_out)
        ctx.elaborate()

        bus_in.data.d = 77
        bus_in.iq.re.d = 9
        ctx.run(1)
        assert bus_out.data.q == -77
        assert bus_out.iq.re.q == -9


def test_signal_struct_from_signals():
    with Context() as ctx:
        stage = Stage("stage")
        data = SignalS16("data", 12)
        re, im = SignalS8("re"), SignalS8("im")

        iq = SignalStruct("iq").add("re", re).add("im", im)
        bus = SignalStruct("bus")
        bus.add("data", data).add("valid", SignalU8("valid", 1))
        bus.add("taps", SignalArrayU8("taps", (4,), 6)).add("iq", iq)
        assert bus.fields == ["data", "valid", "taps", "iq"]
        # The struct refers to the signals that were added.
        assert bus.data is data
        assert bus.iq.im is im
        # It keeps the ones that nothing else refers to.
        assert isinstance(bus.valid, SignalU8)
        assert bus.taps.shape == (4,)

        with pytest.raises(ValueError, match="already has a field 'data'"):
            bus.add("data", data)
        with pytest.raises(TypeError, match="A field of a SignalStruct"):
            bus.add("number", 5)
        with pytest.raises(AttributeError, match="has no field 'ready'"):
            _ = bus.ready

        bus_out = stage.bus_out.signals("bus_out")
        stage.bus_in.bind(bus)
        stage.bus_out.bind(bus_out)
        ctx.elaborate()
        data.d = 40
        im.d = 3
        ctx.run(1)
        assert bus_out.data.q == -40
        assert bus_out.iq.im.q == -3


def test_struct_bind_errors():
    with Context() as ctx:
        stage = Stage("stage")
        iq = SignalStruct("iq").add("re", SignalS8("re")).add("q", SignalS8("q"))
        bus = SignalStruct("bus")
        bus.add("data", SignalS32("wrong")).add("taps", SignalArrayU8("taps", (4,)))
        bus.add("iq", iq).add("extra", SignalU8("extra"))

        # Every difference is reported, with the path of the field.
        with pytest.raises(ValueError) as error:
            stage.bus_in.bind(bus)
        message = str(error.value)
        assert "Cannot bind port struct 'bus_in' to 'bus'" in message
        assert (
            "field 'data' of the signals does not have the type of the port" in message
        )
        assert "no field 'valid' in the signals" in message
        assert "no field 'iq.im' in the signals" in message
        assert "no field 'iq.q' in the ports" in message
        assert "no field 'extra' in the ports" in message

        # An input struct does not bind to an output struct.
        with pytest.raises(ValueError, match="has another type"):
            stage.bus_in.bind(stage.bus_out)

        # Nothing was bound by the failed attempts.
        bus_in = stage.bus_in.signals("bus_in")
        bus_out = stage.bus_out.signals("bus_out")
        stage.bus_in.bind(bus_in)
        stage.bus_out.bind(bus_out)
        ctx.elaborate()
        bus_in.data.d = 2
        ctx.run(1)
        assert bus_out.data.q == -2


class ArrayStage(Module):
    """Two rows of three buses in and out. The data of bus [i, j] is offset by 3 * i + j."""

    def __init__(self, name: str):
        self.buses_in = PortStructArray(
            "buses_in",
            (2, 3),
            [
                bus_ports(
                    f"buses_in[{i}][{j}]", InputS16, InputU8, InputS8, InputArrayU8
                )
                for i in range(2)
                for j in range(3)
            ],
        )
        self.buses_out = PortStructArray(
            "buses_out",
            (2, 3),
            [
                bus_ports(
                    f"buses_out[{i}][{j}]", OutputS16, OutputU8, OutputS8, OutputArrayU8
                )
                for i in range(2)
                for j in range(3)
            ],
        )
        # An array of structs as a field of a struct.
        self.group = PortStruct("group")
        self.group.add("valid", InputU8("group.valid", 1))
        self.group.add("pair", self.buses_in[0, :2])
        self.process(self.eval, "ArrayStage.eval").always("*")

    def eval(self):
        for i in range(2):
            for j in range(3):
                bus_in, bus_out = self.buses_in[i, j], self.buses_out[i, j]
                bus_out.data.d = bus_in.data.q + 3 * i + j
                bus_out.valid.d = bus_in.valid.q
                bus_out.taps[0].d = bus_in.taps[3].q
                bus_out.iq.re.d = -bus_in.iq.re.q


def test_port_struct_array():
    with Context():
        stage = ArrayStage("stage")
        buses = stage.buses_in
        assert (buses.name, buses.shape, buses.ndim, buses.size) == (
            "buses_in",
            (2, 3),
            2,
            6,
        )
        assert buses.fields == ["data", "valid", "taps", "iq"]

        # An element is a struct.
        assert isinstance(buses[1, 2], PortStruct)
        assert buses[1, 2].name == "buses_in[1][2]"
        assert buses[1, 2].data.name == "buses_in[1][2].data"
        assert buses[1, 2] is buses.flat(5)
        assert [bus.name for bus in buses][4] == "buses_in[1][1]"
        with pytest.raises(IndexError):
            buses[0, 3]
        with pytest.raises(TypeError, match="integers, slices or the name of a field"):
            buses[1.5]

        # A part of the array.
        assert buses[1].shape == (3,)
        assert buses[1][2] is buses[1, 2]
        assert buses[:, 1].shape == (2,)
        assert buses[-1, -1] is buses[1, 2]

        # One field of every element.
        assert isinstance(buses.data, InputArrayViewS16)
        assert buses.data.shape == (2, 3)
        assert buses.data[1, 2] is buses[1, 2].data
        assert buses["data"][0, 0] is buses[0, 0].data
        assert buses.taps.shape == (2, 3, 4)
        assert buses.iq.shape == (2, 3)
        assert buses.iq.re[1, 0] is buses[1, 0].iq.re
        assert stage.buses_out.data.shape == (2, 3)
        with pytest.raises(AttributeError, match="has no field 'ready'"):
            _ = buses.ready

        # As a field of a struct.
        assert isinstance(stage.group.pair, PortStructArray)
        assert stage.group.pair.shape == (2,)
        assert stage.group.pair[1] is buses[0, 1]

        with pytest.raises(ValueError, match="not what its shape holds"):
            PortStructArray("bad", (2, 2), [buses[0, 0]])


def test_struct_array_bind():
    with Context() as ctx:
        stage = ArrayStage("stage")
        buses_in = stage.buses_in.signals("buses_in")
        buses_out = stage.buses_out.signals("buses_out")
        assert isinstance(buses_in, SignalStructArray)
        assert (buses_in.name, buses_in.shape) == ("buses_in", (2, 3))
        assert isinstance(buses_in[1, 2], SignalStruct)
        assert buses_in[1, 2].name == "buses_in[1][2]"
        assert buses_in[1, 2].data.name == "buses_in[1][2].data"
        assert buses_in[1, 2].data.width == 12
        stage.buses_in.bind(buses_in)
        stage.buses_out.bind(buses_out)
        ctx.elaborate()

        # Element by element, and field by field.
        buses_in[1, 2].data.d = 100
        buses_in[0, 1].taps[3].d = 9
        assert isinstance(buses_in.iq.re, SignalArrayViewS8)
        buses_in.iq.re.d = [[1, 2, 3], [4, 5, 6]]
        buses_in.valid.d = 1
        ctx.run(1)

        assert buses_out.data.q.tolist() == [[0, 1, 2], [3, 4, 105]]
        assert buses_out[1, 2].data.q == 105
        assert buses_out[0, 1].taps[0].q == 9
        assert buses_out.taps.shape == (2, 3, 4)
        assert buses_out.iq.re.q.tolist() == [[-1, -2, -3], [-4, -5, -6]]
        assert [bus.valid.q for bus in buses_out] == [1] * 6
        # A part of the array refers to the same structs.
        assert buses_out[1][2].data is buses_out[1, 2].data
        assert buses_out[1].data.q.tolist() == [3, 4, 105]


def test_struct_array_from_structs():
    with Context() as ctx:
        stage = ArrayStage("stage")
        # The signals of each element, put together by hand.
        structs = [bus.signals(f"s{i}") for i, bus in enumerate(stage.buses_in)]
        buses_in = SignalStructArray("buses_in", (2, 3), structs)
        assert buses_in[1, 0].data is structs[3].data
        assert buses_in.data.shape == (2, 3)
        buses_out = stage.buses_out.signals("buses_out")

        # A struct with an array of structs as a field, and the signals for it.
        group = SignalStruct("group")
        group.add("valid", SignalU8("valid", 1)).add("pair", buses_in[0, :2])
        assert isinstance(group.pair, SignalStructArray)
        assert group.pair[1].data is structs[1].data
        assert stage.group.signals("made").pair.shape == (2,)

        # Another shape, and an element with other fields.
        with pytest.raises(
            ValueError, match=r"shape \(2, 3\) in the ports and \(3, 2\)"
        ):
            stage.buses_in.bind(SignalStructArray("reshaped", (3, 2), structs))
        odd = [*structs[:5], SignalStruct("odd").add("data", SignalS16("odd_data"))]
        with pytest.raises(
            ValueError, match=r"no field '\[1\]\[2\].valid' in the signals"
        ):
            stage.buses_in.bind(SignalStructArray("odd", (2, 3), odd))
        with pytest.raises(ValueError, match="has another type"):
            stage.buses_in.bind(stage.buses_out)
        with pytest.raises(ValueError, match="not what its shape holds"):
            SignalStructArray("bad", (4,), structs)

        # Nothing was bound by the failed attempts.
        stage.buses_in.bind(buses_in)
        stage.buses_out.bind(buses_out)
        ctx.elaborate()
        structs[2].data.d = 5
        ctx.run(1)
        assert buses_out[0, 2].data.q == 7


def test_struct_array_views_are_released():
    """
    A struct array that Python builds keeps its structs and their ports alive, and a view of one of its
    fields keeps the array alive. An element that is read from the view must not keep the view alive as
    well, or none of them is ever released, which nanobind reports when the interpreter exits.
    """
    code = """
from dspsim.framework import Context, InputS16, Module, PortStruct, PortStructArray

class Stage(Module):
    def __init__(self, name):
        structs = [PortStruct(f"bus[{i}]").add("data", InputS16(f"bus[{i}].data")) for i in range(2)]
        self.bus = PortStructArray("bus", (2,), structs)

with Context():
    stage = Stage("stage")
    assert stage.bus.data[1] is stage.bus[1].data
    assert stage.bus[0:1].data[0] is stage.bus[0].data
"""
    result = subprocess.run(
        [sys.executable, "-c", code], capture_output=True, text=True, check=True
    )
    assert "leaked" not in result.stderr, result.stderr
