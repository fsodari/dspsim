from pathlib import Path

from dspsim.builder import build_vmodule, vbuilder
from dspsim.framework import (
    Clock,
    Context,
    Input8,
    Input16,
    # Input32,
    # Input64,
    Module,
    Output8,
    Output16,
    # Output32,
    # Output64,
    Signal8,
    Signal16,
)


def test_vbuilder():
    print()
    hdl_dir = Path.cwd() / "tests" / "test_modules"
    source = hdl_dir / "Skid.sv"

    @vbuilder(source=source, include_dirs=[hdl_dir], trace="platform")
    class Skid1(Module):
        # Parameters
        DW: int = 12
        # Ports
        clk: Input8
        rst: Input8
        s_axis_tdata: Input16
        s_axis_tvalid: Input8
        s_axis_tready: Output8
        m_axis_tdata: Output16
        m_axis_tvalid: Output8
        m_axis_tready: Input8

    print("Skid defined.")
    Skid2 = build_vmodule(
        "Skid2",
        source=source,
        include_dirs=[hdl_dir],
        parameters={"DW": 13},
        trace="platform",
    )

    with Context("test_vbuilder") as context:
        print(context.name)
        context.log_level = "debug"

        clk = Clock("clk", 10)
        rst = Signal8("rst")

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
        skid1.open_trace("traces/skid1.fst")
        skid1.clk.bind(clk)
        skid1.rst.bind(rst)
        skid1.s_axis_tdata.bind(in_tdata)
        skid1.s_axis_tvalid.bind(in_tvalid)
        skid1.s_axis_tready.bind(in_tready)
        skid1.m_axis_tdata.bind(skid1_tdata)
        skid1.m_axis_tvalid.bind(skid1_tvalid)
        skid1.m_axis_tready.bind(skid1_tready)

        skid2 = Skid2("skid2")
        skid2.open_trace("traces/skid2.fst")
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
        assert skid1.DW == 12
        assert skid1.s_axis_tdata.width == 12

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
        context.run(100)
