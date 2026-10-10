"""Stimulus and settings shared by the dspsim and cocotb testbenches."""

import json
import os
import random
from dataclasses import dataclass
from pathlib import Path

HDL_TOPLEVEL = "Skid"
HDL_SOURCE = Path(__file__).parent.parent / "hdl" / "Skid.sv"

# Data width of the Skid buffer.
DW = 16
# Clock period in time units (ns in cocotb).
CLK_PERIOD = 10
RESET_CYCLES = 4

SCENARIOS = ("clock", "edge", "axis")
SCENARIO_HELP = {
    "clock": "free-running clock, no Python activity per cycle",
    "edge": "one Python coroutine woken on every rising edge",
    "axis": "Python AXI-Stream driver + monitor with random valid/ready and a data check",
}

# Lengths of the repeating valid/ready patterns. Coprime, so the combined pattern is long.
_VALID_PATTERN_LEN = 1021
_READY_PATTERN_LEN = 1031


@dataclass
class Stimulus:
    """Stimulus for the axis scenario: the words to send and the handshake patterns."""

    data: list[int]
    valid: list[int]
    ready: list[int]

    @classmethod
    def create(cls, n: int, seed: int = 1234):
        rng = random.Random(seed)
        lo, hi = -(1 << (DW - 1)), (1 << (DW - 1)) - 1
        return cls(
            data=[rng.randint(lo, hi) for _ in range(n)],
            valid=[int(rng.random() < 0.75) for _ in range(_VALID_PATTERN_LEN)],
            ready=[int(rng.random() < 0.75) for _ in range(_READY_PATTERN_LEN)],
        )


def bench_size() -> int:
    """Number of clock cycles (clock, edge) or words (axis) to simulate."""
    return int(os.environ["BENCH_SIZE"])


def write_result(sim: str, scenario: str, sim_time: int, run_time: float):
    """
    Write the result of the measured region to the file the runner asked for.

    sim_time is the simulated time the region covered. Both testbenches must report the same.
    """
    result = {
        "sim": sim,
        "scenario": scenario,
        "cycles": sim_time // CLK_PERIOD,
        "run_time": run_time,
    }
    Path(os.environ["BENCH_RESULT"]).write_text(json.dumps(result))
