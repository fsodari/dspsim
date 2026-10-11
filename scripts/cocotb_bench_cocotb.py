"""cocotb testbench for the Skid buffer. Mirrors cocotb_bench_dspsim.py."""

import time

import cocotb
from cocotb.clock import Clock
from cocotb.simtime import get_sim_time
from cocotb.triggers import FallingEdge, RisingEdge, Timer
from cocotb_bench_common import (
    CLK_PERIOD,
    RESET_CYCLES,
    Stimulus,
    bench_size,
    write_result,
)


def sim_time() -> int:
    return int(get_sim_time("ns"))


async def reset(dut):
    Clock(dut.clk, CLK_PERIOD, unit="ns").start()
    dut.rst.value = 1
    dut.s_axis_tdata.value = 0
    dut.s_axis_tvalid.value = 0
    dut.m_axis_tready.value = 0
    await Timer(RESET_CYCLES * CLK_PERIOD, unit="ns")
    dut.rst.value = 0


@cocotb.test()
async def clock(dut):
    n = bench_size()
    await reset(dut)

    t0, start = sim_time(), time.perf_counter()
    await Timer(n * CLK_PERIOD, unit="ns")
    run_time = time.perf_counter() - start
    write_result("cocotb", "clock", sim_time() - t0, run_time)


@cocotb.test()
async def edge(dut):
    n = bench_size()
    await reset(dut)
    posedge = RisingEdge(dut.clk)

    t0, start = sim_time(), time.perf_counter()
    for _ in range(n):
        await posedge
    run_time = time.perf_counter() - start
    write_result("cocotb", "edge", sim_time() - t0, run_time)


async def driver(dut, stim: Stimulus):
    """Offer each word with a random valid pattern. Signals change on the falling edge."""
    negedge = FallingEdge(dut.clk)
    tdata, tvalid, tready = dut.s_axis_tdata, dut.s_axis_tvalid, dut.s_axis_tready
    valid, n_valid = stim.valid, len(stim.valid)
    cycle = 0
    for word in stim.data:
        while True:
            await negedge
            v = valid[cycle % n_valid]
            cycle += 1
            tvalid.value = v
            if v:
                tdata.value = word
                # Nothing changes until the next falling edge: the word is accepted on the rising edge.
                if tready.value:
                    break
    await negedge
    tvalid.value = 0


async def monitor(dut, stim: Stimulus) -> list[int]:
    """Apply a random ready pattern and collect the words that are transferred."""
    negedge = FallingEdge(dut.clk)
    tdata, tvalid, tready = dut.m_axis_tdata, dut.m_axis_tvalid, dut.m_axis_tready
    ready, n_ready = stim.ready, len(stim.ready)
    n = len(stim.data)
    received = []
    cycle = 0
    while len(received) < n:
        await negedge
        r = ready[cycle % n_ready]
        cycle += 1
        tready.value = r
        if r and tvalid.value:
            received.append(tdata.value.to_signed())
    return received


@cocotb.test()
async def axis(dut):
    stim = Stimulus.create(bench_size())
    await reset(dut)

    t0, start = sim_time(), time.perf_counter()
    cocotb.start_soon(driver(dut, stim))
    received = await monitor(dut, stim)
    run_time = time.perf_counter() - start

    assert received == stim.data
    write_result("cocotb", "axis", sim_time() - t0, run_time)
