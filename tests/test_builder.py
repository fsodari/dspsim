import sys
from pathlib import Path

import pytest

from dspsim.builder import VModule, build_vmodule, vbuilder
from dspsim.framework import (
    Clock,
    Context,
    InputArrayU16,
    InputS16,
    InputU8,
    InputU16,
    Module,
    OutputArrayU16,
    # Input32,
    # Input64,
    # Module,
    OutputS16,
    OutputU8,
    SignalArrayU16,
    # Output32,
    # Output64,
    SignalS16,
    SignalU8,
)

HDL_DIR = Path(__file__).parent.parent / "hdl"

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
