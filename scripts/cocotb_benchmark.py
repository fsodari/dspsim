#!/usr/bin/env python3
"""
Compare dspsim against cocotb: the same SystemVerilog module (hdl/Skid.sv), verilated for both,
driven by equivalent Python testbenches (cocotb_bench_dspsim.py, cocotb_bench_cocotb.py).

    uv run scripts/cocotb_benchmark.py
    uv run scripts/cocotb_benchmark.py --scenario axis --size 500000 --repeat 5
"""

import argparse
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

import cocotb_bench_common as common
import cocotb_bench_dspsim
import cocotb_tools.runner

SCRIPTS_DIR = Path(__file__).parent

SIMS = ("dspsim", "cocotb")


@dataclass
class Result:
    """Best of the repeated runs of one scenario on one simulator."""

    cycles: int
    # Wall time of the measured region inside the testbench.
    run_time: float
    # Wall time of the whole simulator process, including startup.
    total_time: float

    @property
    def rate(self) -> float:
        """Simulated clock cycles per second."""
        return self.cycles / self.run_time


class DspsimBench:
    def build(self, verbose: bool):
        cocotb_bench_dspsim.build_skid(verbose=verbose)

    def run(self, scenario: str, env: dict[str, str]):
        subprocess.run(
            [sys.executable, str(SCRIPTS_DIR / "cocotb_bench_dspsim.py"), scenario],
            env=env,
            check=True,
        )


class CocotbBench:
    def __init__(self, build_dir: Path):
        self.build_dir = build_dir / "cocotb"
        self.runner: cocotb_tools.runner.Runner

    def build(self, verbose: bool):
        from cocotb_tools.runner import get_runner

        from dspsim.verilator import verilator_root

        # cocotb looks for the verilator executable on the PATH.
        os.environ["PATH"] = (
            f"{verilator_root() / 'bin'}{os.pathsep}{os.environ['PATH']}"
        )

        self.runner = get_runner("verilator")
        self.runner.build(
            sources=[common.HDL_SOURCE],
            includes=[common.HDL_SOURCE.parent],
            hdl_toplevel=common.HDL_TOPLEVEL,
            parameters={"DW": common.DW},
            # Same compiler optimization as dspsim's Release build of the model.
            build_args=["-CFLAGS", "-O3 -DNDEBUG"],
            timescale=("1ns", "1ns"),
            build_dir=self.build_dir,
            log_file=None if verbose else self.build_dir / "build.log",
        )

    def run(self, scenario: str, env: dict[str, str]):
        from cocotb_tools.check_results import get_results

        results_xml = self.runner.test(
            test_module="cocotb_bench_cocotb",
            hdl_toplevel=common.HDL_TOPLEVEL,
            testcase=scenario,
            extra_env={k: v for k, v in env.items() if k.startswith("BENCH_")},
            build_dir=self.build_dir,
            test_dir=self.build_dir,
            log_file=self.build_dir / f"{scenario}.log",
        )
        num_tests, num_failed = get_results(results_xml)
        if num_tests != 1 or num_failed != 0:
            raise RuntimeError(
                f"cocotb {scenario} failed, see {self.build_dir / scenario}.log"
            )


def measure(bench, scenario: str, size: int, repeat: int, result_file: Path) -> Result:
    env = os.environ | {"BENCH_SIZE": str(size), "BENCH_RESULT": str(result_file)}
    best = None
    for _ in range(repeat):
        result_file.unlink(missing_ok=True)
        start = time.perf_counter()
        bench.run(scenario, env)
        total_time = time.perf_counter() - start

        data = json.loads(result_file.read_text())
        result = Result(data["cycles"], data["run_time"], total_time)
        if best is None or result.run_time < best.run_time:
            best = result
    assert best is not None
    return best


def print_results(results: dict[str, dict[str, Result]], sims: list[str]):
    header = f"{'scenario':<10}{'cycles':>10}"
    for sim in sims:
        header += f"{sim + ' run':>14}{'kcycles/s':>11}{'total':>9}"
    if len(sims) == 2:
        header += f"{'speedup':>10}"
    print()
    print(header)
    print("-" * len(header))
    for scenario, by_sim in results.items():
        line = f"{scenario:<10}{by_sim[sims[0]].cycles:>10}"
        for sim in sims:
            r = by_sim[sim]
            line += f"{r.run_time:>12.4f} s{r.rate / 1e3:>11.0f}{r.total_time:>7.2f} s"
        if len(sims) == 2:
            line += f"{by_sim['cocotb'].run_time / by_sim['dspsim'].run_time:>9.1f}x"
        print(line)
    print()
    print(
        "run: measured region inside the testbench. total: whole process, with startup."
    )
    if len(sims) == 2:
        print("speedup: cocotb run time / dspsim run time.")
    for scenario in results:
        print(f"  {scenario}: {common.SCENARIO_HELP[scenario]}")


def main():
    parser = argparse.ArgumentParser(description=str(__doc__).strip().splitlines()[0])
    parser.add_argument(
        "--scenario",
        action="append",
        choices=common.SCENARIOS,
        help="scenario to run, can be repeated (default: all)",
    )
    parser.add_argument(
        "--sim",
        action="append",
        choices=SIMS,
        help="simulator to run, can be repeated (default: both)",
    )
    parser.add_argument(
        "--size",
        type=int,
        default=200_000,
        help="clock cycles (clock, edge) or words (axis) to simulate",
    )
    parser.add_argument("--repeat", type=int, default=3, help="runs, the best is kept")
    parser.add_argument(
        "--build-dir", type=Path, default=Path("build") / "cocotb_benchmark"
    )
    parser.add_argument("--verbose", action="store_true", help="show the build output")
    args = parser.parse_args()

    scenarios = args.scenario or list(common.SCENARIOS)
    sims = [s for s in SIMS if s in (args.sim or SIMS)]
    build_dir = args.build_dir.resolve()
    build_dir.mkdir(parents=True, exist_ok=True)

    benches = {"dspsim": DspsimBench(), "cocotb": CocotbBench(build_dir)}
    for sim in sims:
        print(f"Building the {sim} model")
        benches[sim].build(args.verbose)

    results: dict[str, dict[str, Result]] = {}
    for scenario in scenarios:
        results[scenario] = {}
        for sim in sims:
            print(f"Running {scenario} on {sim}")
            results[scenario][sim] = measure(
                benches[sim],
                scenario,
                args.size,
                args.repeat,
                build_dir / "result.json",
            )
        # Equivalent testbenches simulate exactly the same number of clock cycles.
        cycles = {r.cycles for r in results[scenario].values()}
        if len(cycles) != 1:
            raise RuntimeError(
                f"{scenario}: the testbenches disagree on cycles: {cycles}"
            )

    print_results(results, sims)


if __name__ == "__main__":
    main()
