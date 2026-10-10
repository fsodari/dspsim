import pytest
from dspsim._framework import DerivedSignalS16, DerivedSignalU8, DerivedSignalU16

from dspsim.framework import (
    BitSel,
    Context,
    InputS32,
    InputU32,
    Module,
    OutputS32,
    SignalArrayU8,
    SignalFloat,
    SignalS16,
    SignalS32,
    SignalU8,
    SignalU16,
    SignalU64,
    bits,
    mask,
    pack,
    sext,
    zext,
)


def test_bit_helpers():
    assert mask(0) == 0
    assert mask(8) == 0xFF
    assert mask(64) == 2**64 - 1
    assert bits(0xABCD, 11, 4) == 0xBC
    assert zext(0xFFF5, 8) == 0xF5
    assert sext(0xFFF5, 16) == -11
    assert sext(0x7FF5, 16) == 0x7FF5
    # Negative and oversized ints are treated as two's complement bits.
    assert zext(-11, 16) == 0xFFF5
    assert sext(2**64 - 11, 64) == -11

    with pytest.raises(ValueError):
        sext(1, 0)
    with pytest.raises(ValueError):
        mask(65)
    with pytest.raises(IndexError):
        bits(1, 3, 4)


def test_slices():
    with Context() as ctx:
        with ctx.construct():
            a = SignalU16("a", init=0xA5C3)
            s = SignalS32("s", width=24, init=-11)

        assert a.width == 16
        assert a[7:0].read() == 0xC3
        assert a[15:8].value == 0xA5
        assert a[0].read() == 1
        assert int(a[11:4]) == 0x5C
        # An omitted bound selects to the end.
        assert a[:8].read() == 0xA5
        assert a[7:].read() == 0xC3
        assert a.slice(3, 0).read() == 0x3
        assert BitSel(a, 3, 0).read() == 0x3
        assert a[11:4][3:0].parts == [(a, 7, 4)]
        assert a[11:4].width == 8

        # Slices are unsigned.
        assert s[23:8].read() == 0xFFFF
        assert sext(s[15:0].read(), 16) == -11
        assert s[15:0].read_signed() == -11
        assert s[23:0].read_signed() == -11
        assert s[7:0].read_signed() == -11

        a[15:8].write(0x12)
        a[7:0].value = 0x34
        ctx.run(0)
        assert a.read() == 0x1234

        # Negative values are written as two's complement bits.
        s[15:0].write(-2)
        ctx.run(0)
        assert s.read() == sext(0xFFFFFE, 24)

        with pytest.raises(IndexError):
            a[16]
        with pytest.raises(IndexError):
            a[3:4]
        with pytest.raises(ValueError):
            a[7:0:2]
        with pytest.raises(TypeError):
            a["x"]


def test_pack():
    with Context() as ctx:
        with ctx.construct():
            tdata = SignalS16("tdata", init=-11)
            tid = SignalU8("tid", init=3)
            arr = SignalArrayU8("arr", (2, 2), width=4)
            arr[0, 0] = 1
            arr[0, 1] = 2
            arr[1, 0] = 3
            arr[1, 1] = 4
            wide = SignalU64("wide")
            real = SignalFloat("real")

        ctx.run(0)
        p = pack(tdata, tid)
        assert p.width == 24
        assert p.read() == 0xFFF503
        assert p.parts == [(tid, 7, 0), (tdata, 15, 0)]
        assert repr(p) == "BitSel({tdata[15:0], tid[7:0]}, width=24)"
        assert sext(p[23:8].read(), 16) == -11

        # Arrays are packed row-major, element 0 most significant. Lists are flattened too.
        assert pack(arr).read() == 0x1234
        assert pack(arr[1]).read() == 0x34
        assert pack([tid, arr[0]], tid[3:0]).read() == 0x03123

        p.write(0x123456)
        ctx.run(0)
        assert tdata.read() == 0x1234
        assert tid.read() == 0x56

        with pytest.raises(TypeError):
            pack(tid, 5)
        with pytest.raises(ValueError):
            pack(wide, tid)
        with pytest.raises(ValueError):
            pack(real)


class Probe(Module):
    def __init__(self, name: str):
        self.packed = InputU32("packed", 16)
        self.signed = InputS32("signed", 24)
        self.out = OutputS32("out", 24)
        self.seen = []
        self.process(self.eval, "eval").always(self.packed)

    def eval(self):
        self.seen.append(self.packed.read())


class Narrow(Module):
    def __init__(self, name: str):
        self.i = InputU32("i", 16)


def test_ports_bind_to_selections():
    with Context() as ctx:
        with ctx.construct():
            a = SignalU8("a", init=0x12)
            b = SignalU8("b", init=0x34)
            tdata = SignalS16("tdata")
            tid = SignalU8("tid")
            probe = Probe("probe")
            probe.packed.bind(pack(a, b))
            probe.signed(pack(a, b[7:0], a))
            probe.out.bind(pack(tdata, tid))

            # The selection must match the port width.
            narrow = Narrow("narrow")
            with pytest.raises(ValueError):
                narrow.i.bind(a)
            narrow.i.bind(pack(a, b))

        ctx.run(0)
        assert probe.packed.read() == 0x1234
        assert probe.signed.read() == 0x123412

        probe.seen.clear()
        a.write(0xAB)
        b.write(0xCD)
        ctx.run(0)
        # Both sources change in the same delta cycle, so the process runs once.
        assert probe.seen == [0xABCD]
        assert probe.signed.read() == sext(0xABCDAB, 24)

        probe.out.write(-11)
        ctx.run(0)
        assert tdata.read() == -1
        assert tid.read() == 0xF5


class EdgeCounter(Module):
    def __init__(self, name: str, trigger):
        self.posedges = 0
        self.process(self.eval, "eval").always(trigger.pos()).initialize(False)

    def eval(self):
        self.posedges += 1


def test_selection_signals():
    with Context() as ctx:
        with ctx.construct():
            a = SignalU16("a")
            bit = a[3].signal("bit3")
            mid = a[15:4].signal()
            counter = EdgeCounter("counter", bit)

        # Python sees the concrete derived signal type.
        assert isinstance(bit, DerivedSignalU8)
        assert isinstance(mid, DerivedSignalU16)
        assert not isinstance(mid, DerivedSignalS16)
        assert bit.name == "bit3"
        assert bit.width == 1
        assert bit.selection.parts == [(a, 3, 3)]

        ctx.run(0)
        a.write(0x0F00)
        ctx.run(0)
        assert counter.posedges == 0
        assert mid.read() == 0x0F0

        a.write(0x0F08)
        ctx.run(0)
        assert counter.posedges == 1
        assert bit.read() == 1

        with pytest.raises(RuntimeError):
            a[0].signal()
