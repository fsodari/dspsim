import random
import sys
from pathlib import Path

import pytest

from dspsim.builder import VModule, build_vmodule, vbuilder
from dspsim.framework import (
    Clock,
    Context,
    InputArrayU8,
    InputArrayU16,
    InputArrayU32,
    InputArrayViewS16,
    InputS8,
    InputS16,
    InputS64,
    InputU8,
    InputU16,
    InputU32,
    InputU64,
    Module,
    OutputArrayU16,
    OutputArrayU32,
    # Input32,
    # Input64,
    # Module,
    OutputS16,
    OutputU8,
    OutputU16,
    OutputU64,
    PortStruct,
    PortStructArray,
    SignalArrayU16,
    SignalArrayU32,
    SignalArrayViewS16,
    # Output32,
    # Output64,
    SignalS16,
    SignalStruct,
    SignalStructArray,
    SignalU8,
    SignalU16,
    from_words,
    signal,
    to_words,
)

HDL_DIR = Path(__file__).parent.parent / "hdl"

# The name of a model that is built is in the path of its generated files four times, and Verilator adds the
# names of the packages of the design. Windows limits a path to 260 characters, and the wheel CI runs the tests
# in a temporary directory with a long path: keep the names of the models here to 13 characters, or to 11 if
# the design has a package.

if sys.platform == "win32":
    TRACE_SUFFIX = ".vcd"
else:
    TRACE_SUFFIX = ".fst"


def test_vbuilder():
    print()
    source = HDL_DIR / "Skid.sv"

    @vbuilder(source=source, include_dirs=[HDL_DIR], trace="platform", verbose=True)
    class Skid1(VModule):
        # Parameters
        DW: int = 14
        # Ports
        clk: InputU8
        rst: InputU8
        s_axis_tdata: InputS16
        s_axis_tvalid: InputU8
        s_axis_tready: OutputU8
        m_axis_tdata: OutputS16
        m_axis_tvalid: OutputU8
        m_axis_tready: InputU8

    Skid2 = build_vmodule(
        "Skid2",
        source=source,
        include_dirs=[HDL_DIR],
        parameters={"DW": 13},
        trace="platform",
        verbose=True,
    )

    with Context("test_vbuilder") as context:
        print(context.name)
        # context.log_level = "debug"

        clk = Clock("clk", 10)
        rst = SignalU8("rst", 1)

        in_tdata = SignalS16("in_tdata")
        in_tvalid = SignalU8("in_tvalid")
        in_tready = SignalU8("in_tready")
        skid1_tdata = SignalS16("skid1_tdata")
        skid1_tvalid = SignalU8("skid1_tvalid")
        skid1_tready = SignalU8("skid1_tready")
        out_tdata = SignalS16("out_tdata")
        out_tvalid = SignalU8("out_tvalid")
        out_tready = SignalU8("out_tready")

        skid1 = Skid1("skid1")

        skid1.open_trace(Path("traces/skid1").with_suffix(TRACE_SUFFIX))
        skid1.clk.bind(clk)
        skid1.rst.bind(rst)
        skid1.s_axis_tdata.bind(in_tdata)
        skid1.s_axis_tvalid.bind(in_tvalid)
        skid1.s_axis_tready.bind(in_tready)
        skid1.m_axis_tdata.bind(skid1_tdata)
        skid1.m_axis_tvalid.bind(skid1_tvalid)
        skid1.m_axis_tready.bind(skid1_tready)

        skid2 = Skid2("skid2")
        skid2.open_trace(Path("traces/skid2").with_suffix(TRACE_SUFFIX))
        skid2.clk.bind(clk)
        skid2.rst.bind(rst)
        skid2.s_axis_tdata.bind(skid1_tdata)
        skid2.s_axis_tvalid.bind(skid1_tvalid)
        skid2.s_axis_tready.bind(skid1_tready)
        skid2.m_axis_tdata.bind(out_tdata)
        skid2.m_axis_tvalid.bind(out_tvalid)
        skid2.m_axis_tready.bind(out_tready)

        assert skid1.context.name == "test_vbuilder"
        assert skid1.__class__.__name__ == "Skid1"
        assert skid1.name == "skid1"
        assert skid1.DW == 14
        assert skid1.s_axis_tdata.width == skid1.DW
        # Signed HDL ports are exposed as signed dspsim ports.
        assert isinstance(skid1.s_axis_tdata, InputS16)
        assert isinstance(skid1.m_axis_tdata, OutputS16)

        assert skid2.context.name == "test_vbuilder"
        assert skid2.__class__.__name__ == "Skid2"
        assert skid2.name == "skid2"
        assert skid2.DW == 13
        assert skid2.s_axis_tdata.width == 13

        context.elaborate()

        rst.d = 1
        context.run(100)
        rst.d = 0
        context.run(100)

        # Negative values must be sign extended from 14 bits and 13 bits.
        in_tdata.d = -99
        in_tvalid.d = 1
        context.run(10)

        out_tready.d = 1
        context.run(10)
        assert out_tvalid.q == 1
        assert out_tdata.q == -99


def test_vbuilder_same_name():
    """Models of the same name are separate builds unless everything about them is the same."""
    source = HDL_DIR / "Skid.sv"

    def build(dw: int):
        return build_vmodule(
            "Skid2",
            source,
            include_dirs=[HDL_DIR],
            parameters={"DW": dw},
            trace="platform",
        )

    Skid13 = build(13)
    Skid12 = build(12)
    assert Skid13 is not Skid12
    assert (Skid13.__name__, Skid12.__name__) == ("Skid2", "Skid2")
    assert (Skid13.DW, Skid12.DW) == (13, 12)
    # The same build again is the class that is already imported.
    assert build(13) is Skid13

    with Context():
        assert Skid13("skid13").s_axis_tdata.width == 13
        assert Skid12("skid12").s_axis_tdata.width == 12


def test_vbuilder_sign_mismatch():
    source = HDL_DIR / "Skid.sv"

    with pytest.raises(TypeError, match="Sign mismatch for port 's_axis_tdata'"):

        @vbuilder(source=source, include_dirs=[HDL_DIR])
        class SkidUnsigned(VModule):
            DW: int = 14
            clk: InputU8
            rst: InputU8
            s_axis_tdata: InputU16
            s_axis_tvalid: InputU8
            s_axis_tready: OutputU8
            m_axis_tdata: OutputS16
            m_axis_tvalid: OutputU8
            m_axis_tready: InputU8


def test_builder_ndarray():
    print()
    source = HDL_DIR / "NDArrayModel.sv"

    @vbuilder(source=source, include_dirs=[HDL_DIR], trace="platform", verbose=True)
    class NDArrayModel(VModule):
        # Parameters
        WIDTH: int = 14
        # Ports
        clk: InputU8
        rst: InputU8
        a: InputArrayU16
        b: OutputArrayU16

    with Context() as context:
        ndarray_model = NDArrayModel("ndarray_model")
        assert ndarray_model.WIDTH == 14
        assert ndarray_model.a.shape == (2, 3, 4)
        assert ndarray_model.b.shape == (2, 3, 4)

        clk = Clock("clk", 10)
        rst = SignalU8("rst", 1)
        a = SignalArrayU16("a", ndarray_model.a.shape)
        b = SignalArrayU16("b", ndarray_model.b.shape)

        ndarray_model.clk.bind(clk)
        ndarray_model.rst.bind(rst)
        ndarray_model.a.bind(a)
        ndarray_model.b.bind(b)

        context.elaborate()
        rst.d = 1
        context.run(10)
        rst.d = 0
        context.run(10)

        # Fill the input array with test data
        for i in range(2):
            for j in range(3):
                for k in range(4):
                    a[i, j, k].d = i * 12 + j * 4 + k
        context.run(10)

        # Check multidimensional access
        for i in range(2):
            for j in range(3):
                for k in range(4):
                    assert b[i, j, k].q == i * 12 + j * 4 + k

        # Check flat iteration over the arrays
        for i, (siga, sigb) in enumerate(zip(a, b)):
            assert sigb.q == siga.q
            assert sigb.q == i


def test_builder_submodule():
    source = HDL_DIR / "Skid.sv"

    @vbuilder(source=source, include_dirs=[HDL_DIR], trace="platform", verbose=True)
    class SkidSubmodule(VModule):
        # Parameters
        DW: int = 14
        # Ports
        clk: InputU8
        rst: InputU8
        s_axis_tdata: InputS16
        s_axis_tvalid: InputU8
        s_axis_tready: OutputU8
        m_axis_tdata: OutputS16
        m_axis_tvalid: OutputU8
        m_axis_tready: InputU8

    class TopModule(Module):
        clk: InputU8
        rst: InputU8
        s_axis_tdata: InputS16
        s_axis_tvalid: InputU8
        s_axis_tready: OutputU8
        m_axis_tdata: OutputS16
        m_axis_tvalid: OutputU8
        m_axis_tready: InputU8

        skid_submodule: SkidSubmodule

        def __init__(self, name: str):
            super().__init__(name)

            self.skid_submodule = SkidSubmodule("skid_submodule")
            self.clk = InputU8("clk", 1)
            self.rst = InputU8("rst", 1)
            self.s_axis_tdata = InputS16("s_axis_tdata", self.skid_submodule.DW)
            self.s_axis_tvalid = InputU8("s_axis_tvalid", 1)
            self.s_axis_tready = OutputU8("s_axis_tready", 1)
            self.m_axis_tdata = OutputS16("m_axis_tdata", self.skid_submodule.DW)
            self.m_axis_tvalid = OutputU8("m_axis_tvalid", 1)
            self.m_axis_tready = InputU8("m_axis_tready", 1)

            self.skid_submodule.clk.bind(self.clk)
            self.skid_submodule.rst.bind(self.rst)
            self.skid_submodule.s_axis_tdata.bind(self.s_axis_tdata)
            self.skid_submodule.s_axis_tvalid.bind(self.s_axis_tvalid)
            self.skid_submodule.s_axis_tready.bind(self.s_axis_tready)
            self.skid_submodule.m_axis_tdata.bind(self.m_axis_tdata)
            self.skid_submodule.m_axis_tvalid.bind(self.m_axis_tvalid)
            self.skid_submodule.m_axis_tready.bind(self.m_axis_tready)

    with Context() as context:
        top_module = TopModule("top_module")

        clk = Clock("clk", 10)
        rst = SignalU8("rst", 1)
        s_axis_tdata = SignalS16("s_axis_tdata", top_module.skid_submodule.DW)
        s_axis_tvalid = SignalU8("s_axis_tvalid", 1)
        s_axis_tready = SignalU8("s_axis_tready", 1)
        m_axis_tdata = SignalS16("m_axis_tdata", top_module.skid_submodule.DW)
        m_axis_tvalid = SignalU8("m_axis_tvalid", 1)
        m_axis_tready = SignalU8("m_axis_tready", 1)

        top_module.clk.bind(clk)
        top_module.rst.bind(rst)
        top_module.s_axis_tdata.bind(s_axis_tdata)
        top_module.s_axis_tvalid.bind(s_axis_tvalid)
        top_module.s_axis_tready.bind(s_axis_tready)
        top_module.m_axis_tdata.bind(m_axis_tdata)
        top_module.m_axis_tvalid.bind(m_axis_tvalid)
        top_module.m_axis_tready.bind(m_axis_tready)
        context.elaborate()

        assert top_module.skid_submodule.DW == 14
        assert s_axis_tdata.width == top_module.skid_submodule.DW
        assert m_axis_tdata.width == top_module.skid_submodule.DW

        rst.d = 1
        context.run(100)
        rst.d = 0
        context.run(100)

        s_axis_tdata.d = -1234
        s_axis_tvalid.d = 1
        context.run(10)
        s_axis_tvalid.d = 0
        context.run(10)

        assert m_axis_tdata.q == -1234
        assert m_axis_tvalid.q == 1


def test_builder_wide_ports():
    """Ports of more than 64 bits are arrays of 32-bit words, least significant first."""
    source = HDL_DIR / "WideModel.sv"

    @vbuilder(source=source, include_dirs=[HDL_DIR])
    class WideModel(VModule):
        W: int = 100
        clk: InputU8
        a: InputArrayU32
        b: InputArrayU32
        sum: OutputArrayU32
        s: InputArrayU32
        neg: OutputArrayU32
        arr_in: InputArrayU32
        arr_out: OutputArrayU32

    with Context() as context:
        clk = Clock("clk", 10)
        dut = WideModel("dut")
        assert isinstance(dut.a, InputArrayU32)
        assert isinstance(dut.sum, OutputArrayU32)
        # The words are the last dimension, after the unpacked dimensions.
        assert dut.a.shape == (4,)
        assert dut.sum.shape == (4,)
        assert dut.arr_in.shape == (3, 4)
        # The most significant word has the bits that remain.
        assert [word.width for word in dut.a] == [32, 32, 32, 4]
        assert dut.sum[3].width == 5

        a = signal("a", width=100)
        b = signal("b", width=100)
        total = signal("sum", width=101)
        s = signal("s", width=100)
        neg = signal("neg", width=100)
        arr_in = SignalArrayU32("arr_in", dut.arr_in.shape)
        arr_out = SignalArrayU32("arr_out", dut.arr_out.shape)
        dut.clk.bind(clk)
        dut.a.bind(a)
        dut.b.bind(b)
        dut.sum.bind(total)
        dut.s.bind(s)
        dut.neg.bind(neg)
        dut.arr_in.bind(arr_in)
        dut.arr_out.bind(arr_out)
        context.elaborate()

        rng = random.Random(1)
        for _ in range(200):
            x, y, z = rng.getrandbits(100), rng.getrandbits(100), rng.getrandbits(99)
            rows = [rng.getrandbits(128) for _ in range(3)]
            a.d = to_words(x, 4)
            b.d = to_words(y, 4)
            s.d = to_words(-z, 4)
            arr_in.d = [to_words(row, 4) for row in rows]
            context.run(10)
            assert from_words(total.q) == x + y
            assert from_words(neg.q, width=100, signed=True) == z
            for i, row in enumerate(rows):
                assert from_words(arr_out[i].q) == row ^ ((1 << 128) - 1)

        # Bits above the port width are not part of the port. Untruncated, they would carry into the sum.
        a.d = [0xFFFFFFFF] * 4
        b.d = [0] * 4
        context.run(10)
        assert from_words(total.q) == (1 << 100) - 1


def test_builder_wide_port_annotation():
    """A wide port is annotated as the array of words that it is."""
    with pytest.raises(TypeError, match="Array mismatch for port 'a'"):

        @vbuilder(source=HDL_DIR / "WideModel.sv", include_dirs=[HDL_DIR])
        class WideModelScalar(VModule):
            clk: InputU8
            a: InputU32
            b: InputArrayU32
            sum: OutputArrayU32
            s: InputArrayU32
            neg: OutputArrayU32
            arr_in: InputArrayU32
            arr_out: OutputArrayU32


def test_builder_struct_ports():
    """
    With struct_ports="fields", each field of a struct port is a port, and a struct binds as a whole to a
    struct of signals. (tests/test_verilated_values.py has the default, a struct as the value of one port.)
    """
    source = HDL_DIR / "StructModel.sv"

    @vbuilder(source=source, include_dirs=[HDL_DIR], struct_ports="fields")
    class StructModel(VModule):
        clk: InputU8
        u_in: PortStruct
        u_out: PortStruct
        p_in: PortStruct
        p_out: PortStruct
        w_in: PortStruct
        w_out: PortStruct

    with Context() as context:
        clk = Clock("clk", 10)
        dut = StructModel("dut")

        assert isinstance(dut.u_in, PortStruct)
        assert dut.u_in.name == "u_in"
        assert dut.u_in.fields == ["data", "valid", "taps", "iq", "wide", "flags"]
        # The fields have the types of ordinary ports.
        assert isinstance(dut.u_in.data, InputS16)
        assert dut.u_in.data.width == 12
        assert dut.u_in.data.name == "u_in.data"
        assert isinstance(dut.u_out.data, OutputS16)
        assert isinstance(dut.u_in.taps, InputArrayU8)
        assert dut.u_in.taps.shape == (4,)
        assert isinstance(dut.u_in.wide, InputArrayU32)
        assert dut.u_in.iq.fields == ["re", "im"]
        assert isinstance(dut.u_in.iq.re, InputS8)
        assert dut.p_in.fields == ["data", "valid", "flags"]
        assert dut.p_in.flags.id.width == 3
        assert isinstance(dut.w_in.mid, InputS64)
        assert dut.w_in.mid.width == 40
        assert dut.w_out.wide.shape == (4,)

        u_in = dut.u_in.signals("u_in")
        u_out = dut.u_out.signals("u_out")
        p_in = dut.p_in.signals("p_in")
        p_out = dut.p_out.signals("p_out")
        w_in = dut.w_in.signals("w_in")
        w_out = dut.w_out.signals("w_out")
        dut.clk.bind(clk)
        dut.u_in.bind(u_in)
        dut.u_out.bind(u_out)
        dut.p_in.bind(p_in)
        dut.p_out.bind(p_out)
        dut.w_in.bind(w_in)
        dut.w_out.bind(w_out)
        context.elaborate()

        rng = random.Random(2)
        all_ones = (1 << 100) - 1
        for _ in range(200):
            # Unpacked struct.
            data, re, im = (
                rng.randrange(-2047, 2048),
                rng.randrange(-127, 128),
                rng.randrange(-127, 128),
            )
            taps = [rng.randrange(64) for _ in range(4)]
            wide, ident, last = rng.getrandbits(100), rng.randrange(8), rng.randrange(2)
            u_in.data.d = data
            u_in.valid.d = last
            u_in.taps.d = taps
            u_in.iq.re.d = re
            u_in.iq.im.d = im
            u_in.wide.d = to_words(wide, 4)
            u_in.flags.id.d = ident
            u_in.flags.last.d = last
            # Packed struct.
            p_data, p_id = rng.randrange(-2047, 2048), rng.randrange(8)
            p_in.data.d = p_data
            p_in.valid.d = last
            p_in.flags.id.d = p_id
            p_in.flags.last.d = last
            # Packed struct of more than 64 bits.
            w_wide, mid, w_id, lo = (
                rng.getrandbits(100),
                rng.randrange(-(2**39) + 1, 2**39),
                rng.randrange(8),
                rng.randrange(8),
            )
            w_in.wide.d = to_words(w_wide, 4)
            w_in.mid.d = mid
            w_in.flags.id.d = w_id
            w_in.flags.last.d = last
            w_in.lo.d = lo
            context.run(10)

            assert u_out.data.q == -data
            assert u_out.valid.q == last
            assert list(u_out.taps.q) == taps[::-1]
            assert (u_out.iq.re.q, u_out.iq.im.q) == (im, re)
            assert from_words(u_out.wide.q) == wide ^ all_ones
            assert u_out.flags.id.q == (ident + 1) % 8
            assert u_out.flags.last.q == 1 - last

            assert p_out.data.q == -p_data
            assert p_out.valid.q == last
            assert p_out.flags.id.q == (p_id + 2) % 8
            assert p_out.flags.last.q == 1 - last

            assert from_words(w_out.wide.q) == w_wide ^ all_ones
            assert w_out.mid.q == -mid
            assert w_out.flags.id.q == (w_id + 3) % 8
            assert w_out.flags.last.q == last
            assert w_out.lo.q == (lo + 1) % 8


def test_builder_struct_bind_between_modules():
    """Two modules connect through the struct of signals of one of them."""
    StructModel = build_vmodule(
        "StructPair",
        source=HDL_DIR / "StructModel.sv",
        include_dirs=[HDL_DIR],
        struct_ports="fields",
    )
    with Context() as context:
        clk = Clock("clk", 10)
        a = StructModel("a")
        b = StructModel("b")
        buses = {}
        for kind in "upw":
            first = getattr(a, f"{kind}_in").signals(f"{kind}_first")
            middle = getattr(a, f"{kind}_out").signals(f"{kind}_middle")
            last = getattr(b, f"{kind}_out").signals(f"{kind}_last")
            getattr(a, f"{kind}_in").bind(first)
            getattr(a, f"{kind}_out").bind(middle)
            getattr(b, f"{kind}_in").bind(middle)
            getattr(b, f"{kind}_out").bind(last)
            buses[kind] = (first, last)
        a.clk.bind(clk)
        b.clk.bind(clk)
        context.elaborate()

        (u_first, u_last), (p_first, p_last), (w_first, w_last) = buses.values()
        u_first.data.d = -321
        u_first.taps.d = [1, 2, 3, 4]
        u_first.iq.re.d = 11
        u_first.wide.d = to_words(1 << 99, 4)
        p_first.data.d = 654
        p_first.flags.id.d = 1
        w_first.mid.d = -(1 << 38)
        w_first.lo.d = 5
        context.run(30)

        # Two stages: negated twice, reversed twice, swapped twice, inverted twice.
        assert u_last.data.q == -321
        assert list(u_last.taps.q) == [1, 2, 3, 4]
        assert u_last.iq.re.q == 11
        assert from_words(u_last.wide.q) == 1 << 99
        assert u_last.flags.id.q == 2
        assert p_last.data.q == 654
        assert p_last.flags.id.q == 5
        assert w_last.mid.q == -(1 << 38)
        assert w_last.lo.q == 7


def test_builder_struct_port_annotation():
    """A struct port is annotated as a PortStruct."""
    with pytest.raises(TypeError, match="Struct mismatch for port 'u_in'"):

        @vbuilder(
            source=HDL_DIR / "StructModel.sv",
            include_dirs=[HDL_DIR],
            struct_ports="fields",
        )
        class StructModelPort(VModule):
            clk: InputU8
            u_in: InputU8
            u_out: PortStruct
            p_in: PortStruct
            p_out: PortStruct
            w_in: PortStruct
            w_out: PortStruct

    with pytest.raises(TypeError, match="Struct mismatch for port 'clk'"):

        @vbuilder(
            source=HDL_DIR / "StructModel.sv",
            include_dirs=[HDL_DIR],
            struct_ports="fields",
        )
        class StructModelStruct(VModule):
            clk: PortStruct
            u_in: PortStruct
            u_out: PortStruct
            p_in: PortStruct
            p_out: PortStruct
            w_in: PortStruct
            w_out: PortStruct


def test_builder_verilator_args(tmp_path, monkeypatch):
    """Extra Verilator arguments apply when the module is read and when it is verilated."""
    # The source is in a temporary directory, whose path is part of the build key.
    monkeypatch.setenv("DSPSIM_CACHE_DIR", str(tmp_path / "cache"))
    source = tmp_path / "Defined.sv"
    source.write_text("""
module Defined (
`ifdef WIDE_DATA
    input  logic [`WIDE_DATA-1:0] d,
    output logic [`WIDE_DATA-1:0] q
`else
    input  logic [7:0] d,
    output logic [7:0] q
`endif
);
  assign q = ~d;
endmodule
""")

    def build(args):
        return build_vmodule(
            "Defined", source=source, include_dirs=[tmp_path], verilator_args=args
        )

    Narrow = build(None)
    Wide = build(["-DWIDE_DATA=20"])
    # The arguments are part of what identifies a build.
    assert Wide is not Narrow
    assert build(["-DWIDE_DATA=20"]) is Wide

    with Context() as context:
        narrow, wide = Narrow("narrow"), Wide("wide")
        assert isinstance(narrow.d, InputU8)
        assert isinstance(wide.d, InputU32)
        assert wide.d.width == 20
        d, q = signal("d", width=20), signal("q", width=20)
        wide.d.bind(d)
        wide.q.bind(q)
        narrow.d.bind(SignalU8("narrow_d"))
        narrow.q.bind(SignalU8("narrow_q"))
        context.elaborate()
        d.d = 0x12345
        context.run(1)
        assert q.q == 0x12345 ^ 0xFFFFF

    # The decorator takes them as well.
    @vbuilder(source=source, include_dirs=[tmp_path], verilator_args=["-DWIDE_DATA=40"])
    class Defined(VModule):
        d: InputU64
        q: OutputU64

    with Context():
        assert Defined("defined").q.width == 40


def test_builder_trace_structs(tmp_path):
    """--trace-structs, passed on to Verilator, shows the fields of a packed struct in the trace."""

    def traced(name, args):
        Model = build_vmodule(
            name,
            source=HDL_DIR / "StructModel.sv",
            include_dirs=[HDL_DIR],
            trace="vcd",
            verilator_args=args,
        )
        with Context() as context:
            clk = Clock("clk", 10)
            dut = Model("dut")
            dut.clk.bind(clk)
            for port in ("u_in", "u_out", "p_in", "p_out", "w_in", "w_out"):
                getattr(dut, port).bind(getattr(dut, port).signal(port))
            dut.open_trace(tmp_path / f"{name}.vcd")
            context.elaborate()
            context.run(20)
            dut.close_trace()
        return (tmp_path / f"{name}.vcd").read_text()

    # A packed struct is one vector by default.
    plain = traced("TracePlain", None)
    assert "p_in [16:0]" in plain
    assert "$scope module p_in" not in plain
    split = traced("TraceSplit", ["--trace-structs"])
    assert "p_in [16:0]" not in split
    assert "$scope module p_in" in split


def test_builder_struct_arrays():
    """An array of structs is an array whose elements are structs. A packed union is a vector of bits."""
    source = HDL_DIR / "StructArrayModel.sv"

    # A short name: see the note on names at the top of this file.
    @vbuilder(source=source, include_dirs=[HDL_DIR], struct_ports="fields")
    class StructArray(VModule):
        clk: InputU8
        a_in: PortStructArray
        a_out: PortStructArray
        f_in: PortStructArray
        f_out: PortStructArray
        w_in: PortStructArray
        w_out: PortStructArray
        o_in: PortStruct
        o_out: PortStruct
        u_in: InputU16
        u_out: OutputU16
        pk_in: InputU16
        pk_out: OutputU16

    with Context() as context:
        clk = Clock("clk", 10)
        dut = StructArray("dut")

        # The elements are structs, named after their place in the array.
        assert isinstance(dut.a_in, PortStructArray)
        assert (dut.a_in.name, dut.a_in.shape, dut.a_in.size) == ("a_in", (2, 3), 6)
        assert len(dut.a_in) == 6
        assert dut.a_in.fields == ["data", "taps", "iq", "wide", "flags"]
        element = dut.a_in[1, 2]
        assert isinstance(element, PortStruct)
        assert element.name == "a_in[1][2]"
        assert isinstance(element.data, InputS16)
        assert (element.data.name, element.data.width) == ("a_in[1][2].data", 12)
        assert element.taps.shape == (2,)
        assert element.wide.shape == (4,)
        assert element.iq.re.name == "a_in[1][2].iq.re"
        assert dut.a_in[1, 2] is element
        assert dut.a_in.flat(5) is element
        assert [s.name for s in dut.f_in] == [f"f_in[{i}]" for i in range(4)]
        with pytest.raises(IndexError):
            dut.a_in[2, 0]
        # Fewer indices, or slices, give a part of the array.
        row = dut.a_in[1]
        assert isinstance(row, PortStructArray)
        assert row.shape == (3,)
        assert row[2] is element
        assert dut.a_in[:, 1:].shape == (2, 2)
        assert dut.a_in[:, 1:][1, 1] is element

        # A field name gives that field of every element as one array, as a struct of arrays has it.
        assert isinstance(dut.a_in.data, InputArrayViewS16)
        assert dut.a_in.data.shape == (2, 3)
        assert dut.a_in.data[1, 2] is element.data
        assert dut.a_in["data"][1, 2] is element.data
        # The dimensions of a field that is an array, and the words of a wide field, come last.
        assert dut.a_in.taps.shape == (2, 3, 2)
        assert dut.a_in.wide.shape == (2, 3, 4)
        assert isinstance(dut.a_in.iq, PortStructArray)
        assert dut.a_in.iq.re.shape == (2, 3)
        assert dut.a_in.flags.id.shape == (2, 3)
        assert dut.w_in.wide.shape == (2, 3)
        assert dut.w_in[1].wide[2].width == 6
        with pytest.raises(AttributeError, match="has no field 'ready'"):
            _ = dut.a_in.ready

        # Arrays of structs that are fields of a struct.
        assert dut.o_in.fields == ["iqs", "flags", "valid"]
        assert isinstance(dut.o_in.iqs, PortStructArray)
        assert dut.o_in.iqs.shape == (3,)
        assert dut.o_in.iqs[2].re.name == "o_in.iqs[2].re"
        assert dut.o_in.flags.id.shape == (2,)
        # A packed union and a packed array of structs are vectors.
        assert dut.u_in.width == 16
        assert dut.pk_in.width == 12

        signals = {}
        for name in ("a", "f", "w", "o"):
            for direction in ("in", "out"):
                port = getattr(dut, f"{name}_{direction}")
                signals[f"{name}_{direction}"] = port.signals(f"{name}_{direction}")
                port.bind(signals[f"{name}_{direction}"])
        u_in, u_out = SignalU16("u_in"), SignalU16("u_out")
        pk_in, pk_out = SignalU16("pk_in", 12), SignalU16("pk_out", 12)
        dut.clk.bind(clk)
        dut.u_in.bind(u_in)
        dut.u_out.bind(u_out)
        dut.pk_in.bind(pk_in)
        dut.pk_out.bind(pk_out)
        context.elaborate()
        a_in, a_out = signals["a_in"], signals["a_out"]
        f_in, f_out = signals["f_in"], signals["f_out"]
        w_in, w_out = signals["w_in"], signals["w_out"]
        o_in, o_out = signals["o_in"], signals["o_out"]
        assert isinstance(a_in, SignalStructArray)
        assert (a_in.name, a_in.shape) == ("a_in", (2, 3))
        assert isinstance(a_in[1, 2], SignalStruct)
        assert a_in[1, 2].data.name == "a_in[1][2].data"
        assert isinstance(a_in.data, SignalArrayViewS16)
        assert a_in.data[1, 2] is a_in[1, 2].data
        assert isinstance(o_in.iqs, SignalStructArray)

        rng = random.Random(3)
        index = [(i, j) for i in range(2) for j in range(3)]
        for _ in range(50):
            # 2 x 3 unpacked structs, element by element.
            data = {ij: rng.randrange(-1000, 1000) for ij in index}
            taps = {ij: [rng.randrange(64), rng.randrange(64)] for ij in index}
            iq = {
                ij: (rng.randrange(-127, 128), rng.randrange(-100, 100)) for ij in index
            }
            wide = {ij: rng.getrandbits(100) for ij in index}
            flags = {ij: (rng.randrange(8), rng.randrange(2)) for ij in index}
            for ij in index:
                element = a_in[ij]
                element.data.d = data[ij]
                element.taps.d = taps[ij]
                element.iq.re.d, element.iq.im.d = iq[ij]
                element.wide.d = to_words(wide[ij], 4)
                element.flags.id.d, element.flags.last.d = flags[ij]
            # 4 packed structs, field by field: one value per element.
            f_id = [rng.randrange(8) for _ in range(4)]
            f_last = [rng.randrange(2) for _ in range(4)]
            f_in.id.d = f_id
            f_in.last.d = f_last
            # 2 packed structs of 86 bits.
            w_wide = [rng.getrandbits(70) for _ in range(2)]
            w_data = [rng.randrange(-1000, 1000) for _ in range(2)]
            w_id = [rng.randrange(8) for _ in range(2)]
            w_in.wide.d = [to_words(x, 3) for x in w_wide]
            w_in.data.d = w_data
            w_in.flags.id.d = w_id
            w_in[0].flags.last.d = 1
            # Arrays of structs in a struct.
            o_re = [rng.randrange(-100, 100) for _ in range(3)]
            o_im = [rng.randrange(-100, 100) for _ in range(3)]
            o_id = [rng.randrange(8) for _ in range(2)]
            o_in.iqs.re.d = o_re
            o_in.iqs.im.d = o_im
            o_in.flags.id.d = o_id
            o_in.flags[1].last.d = 1
            o_in.valid.d = 1
            # The union {op[15:12], imm[11:0]} and the packed array of three 4-bit structs.
            imm, low = rng.randrange(1, 2048), rng.getrandbits(12)
            u_in.d = (rng.randrange(16) << 12) | imm
            pk_in.d = low
            context.run(10)

            for i, j in index:
                element = a_out[i, j]
                assert element.data.q == data[i, j] + 3 * i + j
                assert list(element.taps.q) == taps[i, j][::-1]
                assert element.iq.re.q == -iq[i, j][0]
                assert element.iq.im.q == iq[i, j][1] + i
                assert from_words(element.wide.q) == wide[i, j] ^ ((1 << 100) - 1)
                assert element.flags.id.q == (flags[i, j][0] + j) % 8
                assert element.flags.last.q == 1 - flags[i, j][1]
            # The same signals, field by field.
            assert a_out.data.q.tolist() == [
                [data[i, j] + 3 * i + j for j in range(3)] for i in range(2)
            ]
            assert a_out.iq.re.q.tolist() == [
                [-iq[i, j][0] for j in range(3)] for i in range(2)
            ]
            assert list(f_out.id.q) == [(x + i) % 8 for i, x in enumerate(f_id)]
            assert list(f_out.last.q) == f_last
            for i in range(2):
                assert from_words(w_out[i].wide.q) == w_wide[i] ^ ((1 << 70) - 1)
            assert list(w_out.data.q) == [w_data[0], w_data[1] - 1]
            assert list(w_out.flags.id.q) == [(w_id[0] + 1) % 8, (w_id[1] + 2) % 8]
            assert list(w_out.flags.last.q) == [1, 0]
            assert list(o_out.iqs.re.q) == o_im
            assert [s.im.q for s in o_out.iqs] == [x + i for i, x in enumerate(o_re)]
            assert list(o_out.flags.id.q) == o_id[::-1]
            assert list(o_out.flags.last.q) == [1, 0]
            assert o_out.valid.q == 1
            # op is the low 4 bits of the union's first byte, imm is negated in 12 bits.
            assert u_out.q == ((imm & 0xF) << 12) | (-imm & 0xFFF)
            assert pk_out.q == ((low & 0xF) << 8) | (low & 0xF0) | (low >> 8)


def test_builder_struct_array_annotation():
    """An array of structs is annotated as a PortStructArray."""
    with pytest.raises(
        TypeError,
        match=r"Struct mismatch for port 'a_in': expected an array of structs \(PortStructArray\), "
        r"got a struct \(PortStruct\)",
    ):

        @vbuilder(
            source=HDL_DIR / "StructArrayModel.sv",
            include_dirs=[HDL_DIR],
            struct_ports="fields",
        )
        class StructArrayModelStruct(VModule):
            clk: InputU8
            a_in: PortStruct
            a_out: PortStructArray
            f_in: PortStructArray
            f_out: PortStructArray
            w_in: PortStructArray
            w_out: PortStructArray
            o_in: PortStruct
            o_out: PortStruct
            u_in: InputU16
            u_out: OutputU16
            pk_in: InputU16
            pk_out: OutputU16
