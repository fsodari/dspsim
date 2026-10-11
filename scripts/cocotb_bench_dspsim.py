"""dspsim testbench for the Skid buffer. Mirrors cocotb_bench_cocotb.py."""

import sys
import time

from cocotb_bench_common import (
    CLK_PERIOD,
    DW,
    HDL_SOURCE,
    RESET_CYCLES,
    Stimulus,
    bench_size,
    write_result,
)

from dspsim.builder import build_vmodule
from dspsim.framework import Clock, Context, Module, SignalS16, SignalU8


def build_skid(verbose: bool = False):
    """Build (or load from the cache) the verilated Skid model."""
    return build_vmodule(
        "SkidBench",
        HDL_SOURCE,
        [HDL_SOURCE.parent],
        parameters={"DW": DW},
        verbose=verbose,
    )


class Testbench(Module):
    """The DUT with its clock, reset and stream signals."""

    def __init__(self, name: str):
        self.clk = Clock("clk", CLK_PERIOD)
        self.rst = SignalU8("rst", 1, 1)
        self.s_axis_tdata = SignalS16("s_axis_tdata", DW)
        self.s_axis_tvalid = SignalU8("s_axis_tvalid", 1)
        self.s_axis_tready = SignalU8("s_axis_tready", 1)
        self.m_axis_tdata = SignalS16("m_axis_tdata", DW)
        self.m_axis_tvalid = SignalU8("m_axis_tvalid", 1)
        self.m_axis_tready = SignalU8("m_axis_tready", 1)

        self.dut = build_skid()("dut")
        self.dut.clk.bind(self.clk)
        self.dut.rst.bind(self.rst)
        self.dut.s_axis_tdata.bind(self.s_axis_tdata)
        self.dut.s_axis_tvalid.bind(self.s_axis_tvalid)
        self.dut.s_axis_tready.bind(self.s_axis_tready)
        self.dut.m_axis_tdata.bind(self.m_axis_tdata)
        self.dut.m_axis_tvalid.bind(self.m_axis_tvalid)
        self.dut.m_axis_tready.bind(self.m_axis_tready)

    def start_soon(self, coro):
        return self.add_task(coro)


def reset(ctx: Context, tb: Testbench):
    tb.rst.d = 1
    ctx.run(RESET_CYCLES * CLK_PERIOD)
    tb.rst.d = 0


def clock(ctx: Context, tb: Testbench):
    n = bench_size()
    reset(ctx, tb)

    t0, start = ctx.time, time.perf_counter()
    ctx.run(n * CLK_PERIOD)
    run_time = time.perf_counter() - start
    write_result("dspsim", "clock", ctx.time - t0, run_time)


def edge(ctx: Context, tb: Testbench):
    n = bench_size()
    reset(ctx, tb)
    posedge = tb.clk.pos()

    async def main():
        for _ in range(n):
            await ctx.wait(posedge)

    t0, start = ctx.time, time.perf_counter()
    ctx.run_until(main())
    run_time = time.perf_counter() - start
    write_result("dspsim", "edge", ctx.time - t0, run_time)


async def driver(tb: Testbench, stim: Stimulus):
    """Offer each word with a random valid pattern. Signals change on the falling edge."""
    wait, negedge = tb.wait, tb.clk.neg()
    tdata, tvalid, tready = tb.s_axis_tdata, tb.s_axis_tvalid, tb.s_axis_tready
    valid, n_valid = stim.valid, len(stim.valid)
    cycle = 0
    for word in stim.data:
        while True:
            await wait(negedge)
            v = valid[cycle % n_valid]
            cycle += 1
            tvalid.d = v
            if v:
                tdata.d = word
                # Nothing changes until the next falling edge: the word is accepted on the rising edge.
                if tready.q:
                    break
    await wait(negedge)
    tvalid.d = 0


async def monitor(tb: Testbench, stim: Stimulus) -> list[int]:
    """Apply a random ready pattern and collect the words that are transferred."""
    wait, negedge = tb.wait, tb.clk.neg()
    tdata, tvalid, tready = tb.m_axis_tdata, tb.m_axis_tvalid, tb.m_axis_tready
    ready, n_ready = stim.ready, len(stim.ready)
    n = len(stim.data)
    received = []
    cycle = 0
    while len(received) < n:
        await wait(negedge)
        r = ready[cycle % n_ready]
        cycle += 1
        tready.d = r
        if r and tvalid.q:
            received.append(tdata.q)
    return received


def axis(ctx: Context, tb: Testbench):
    stim = Stimulus.create(bench_size())
    reset(ctx, tb)

    t0, start = ctx.time, time.perf_counter()
    tb.start_soon(driver(tb, stim))
    received = ctx.run_until(monitor(tb, stim))
    run_time = time.perf_counter() - start

    assert received == stim.data
    write_result("dspsim", "axis", ctx.time - t0, run_time)


def main():
    scenario = {"clock": clock, "edge": edge, "axis": axis}[sys.argv[1]]
    with Context() as ctx:
        with ctx.construct():
            tb = Testbench("tb")
        scenario(ctx, tb)


if __name__ == "__main__":
    main()
