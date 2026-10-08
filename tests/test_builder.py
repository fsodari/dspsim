import sys
from pathlib import Path

from dspsim.builder import VModule, build_vmodule, vbuilder
from dspsim.framework import (
    Clock,
    Context,
    Input8,
    Input16,
    Input16Array,
    # Input32,
    # Input64,
    # Module,
    Output8,
    Output16,
    Output16Array,
    # Output32,
    # Output64,
    Signal8,
    Signal16,
    Signal16Array,
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
        clk: Input8
        rst: Input8
        s_axis_tdata: Input16
        s_axis_tvalid: Input8
        s_axis_tready: Output8
        m_axis_tdata: Output16
        m_axis_tvalid: Output8
        m_axis_tready: Input8

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
        rst = Signal8("rst", 1)

        in_tdata = Signal16("in_tdata")
        in_tvalid = Signal8("in_tvalid")
        in_tready = Signal8("in_tready")
        skid1_tdata = Signal16("skid1_tdata")
        skid1_tvalid = Signal8("skid1_tvalid")
        skid1_tready = Signal8("skid1_tready")
        out_tdata = Signal16("out_tdata")
        out_tvalid = Signal8("out_tvalid")
        out_tready = Signal8("out_tready")

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

        in_tdata.d = 99
        in_tvalid.d = 1
        context.run(10)

        out_tready.d = 1
        context.run(10)
        assert out_tvalid.q == 1
        assert out_tdata.q == 99


def test_builder_ndarray():
    print()
    source = HDL_DIR / "NDArrayModel.sv"

    @vbuilder(source=source, include_dirs=[HDL_DIR], trace="platform", verbose=True)
    class NDArrayModel(VModule):
        # Parameters
        WIDTH: int = 14
        # Ports
        clk: Input8
        rst: Input8
        a: Input16Array
        b: Output16Array

    with Context() as context:
        ndarray_model = NDArrayModel("ndarray_model")
        assert ndarray_model.WIDTH == 14
        assert ndarray_model.a.shape == (2, 3, 4)
        assert ndarray_model.b.shape == (2, 3, 4)

        clk = Clock("clk", 10)
        rst = Signal8("rst", 1)
        a = Signal16Array("a", ndarray_model.a.shape)
        b = Signal16Array("b", ndarray_model.b.shape)

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
