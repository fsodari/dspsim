#!/usr/bin/env python3
"""
Build and run the C++ benchmarks (benchmarks/) and compare the engines.

    uv run scripts/benchmarks.py --configure        # first run: configure, build, run everything
    uv run scripts/benchmarks.py                    # rebuild and run everything
    uv run scripts/benchmarks.py chain/dff          # only the cases matching a regular expression
    uv run scripts/benchmarks.py --suite chain      # only build and run one suite
    uv run scripts/benchmarks.py --check            # quick: only check that the engines agree
    uv run scripts/benchmarks.py --save base.json   # keep the results ...
    uv run scripts/benchmarks.py --compare base.json  # ... and compare a later run against them

Each benchmark executable is a suite of cases, and one run of it measures one engine of one case in a
fresh process (see benchmarks/harness/bench.h). This script repeats every measurement, checks that the
engines of a case computed the same thing, and reports the medians.

What is measured: CPU cycles and instructions in user mode, per iteration, over the measured region only
(not construction, elaboration or the warm-up). Instructions are exact and repeatable, which makes them
the number to compare a change of dspsim against a baseline. They are not comparable between engines:
cycles are, which is why the engine comparison uses them. Cycles depend on where the data of a design
happens to land in memory, by several percent, so every repetition runs with a different memory layout,
and the report shows the median and the spread of those runs.
"""

import argparse
import json
import os
import platform
import re
import statistics
import subprocess
import sys
import sysconfig
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).parent.parent
# The engine the others are compared against.
REFERENCE_ENGINE = "systemc"
# The kind the other kinds of a group are compared against.
REFERENCE_KIND = "method"
# A case is marked when an engine spends more than this part of its CPU time in the kernel.
KERNEL_SHARE = 0.10
# Lengths of the --layout argument, one per repetition. The length of the arguments of a process moves
# its stack and, through SystemC's copy of them, its heap. Where the data of a design lands relative to
# cache lines and to other data changes the cycle count by several percent, and it is the same in every
# launch of the same command: without this, the repetitions would all measure one arbitrary layout.
LAYOUTS = (
    8,
    40,
    96,
    152,
    208,
    24,
    72,
    128,
    184,
    240,
    56,
    112,
    168,
    224,
    88,
    144,
    200,
    16,
    120,
    176,
)


@dataclass
class Case:
    """One case of a suite, as listed by its executable."""

    suite: str
    exe: Path
    name: str
    group: str
    kind: str
    engines: list[str]
    iters: int
    description: str

    @property
    def path(self) -> str:
        return f"{self.suite}/{self.name}"


@dataclass
class Measurement:
    """The repeated runs of one engine of one case."""

    case: Case
    engine: str
    iters: int
    runs: list[dict] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)

    def per_iter(self, key: str) -> list[float]:
        return [run[key] / run["iters"] for run in self.runs]

    def median(self, key: str) -> float:
        values = self.per_iter(key)
        return statistics.median(values) if values else 0.0

    def spread(self, key: str) -> float:
        """Range of the runs relative to their median."""
        values = self.per_iter(key)
        middle = statistics.median(values) if values else 0.0
        return (max(values) - min(values)) / middle if middle else 0.0

    @property
    def kernel_share(self) -> float:
        """Part of the CPU time of the measured region that was spent in the kernel."""
        shares = [
            run.get("kernel_ns", 0) / run["cpu_ns"]
            for run in self.runs
            if run.get("cpu_ns")
        ]
        return statistics.median(shares) if shares else 0.0

    @property
    def checksums(self) -> set[str]:
        return {run["checksum"] for run in self.runs}

    @property
    def sim_times(self) -> set[int]:
        return {run["sim_time"] for run in self.runs}

    @property
    def counters(self) -> str:
        return self.runs[0]["counters"] if self.runs else "none"

    def summary(self) -> dict:
        """What is saved for a later comparison."""
        return {
            "suite": self.case.suite,
            "case": self.case.name,
            "group": self.case.group,
            "kind": self.case.kind,
            "engine": self.engine,
            "iters": self.iters,
            "runs": len(self.runs),
            "checksum": sorted(self.checksums),
            "counters": self.counters,
            "instructions": self.median("instructions"),
            "cycles": self.median("cycles"),
            "cycles_spread": self.spread("cycles"),
            "wall_ns": self.median("wall_ns"),
            "cpu_ns": self.median("cpu_ns"),
            "kernel_share": self.kernel_share,
            "l1d_misses": self.median("l1d_misses"),
            "branch_misses": self.median("branch_misses"),
            "build_ns": statistics.median(r["build_ns"] for r in self.runs),
            "start_ns": statistics.median(r["start_ns"] for r in self.runs),
            "peak_rss_kb": max(r["peak_rss_kb"] for r in self.runs),
        }


def parse_args():
    parser = argparse.ArgumentParser(
        description="Build and run the C++ benchmarks and compare the engines.",
        epilog=str(__doc__).split("\n\n", 2)[1],
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "patterns",
        nargs="*",
        help="regular expressions: run the cases whose suite/group/kind matches any of them",
    )
    parser.add_argument("--configure", action="store_true", help="run cmake first")
    parser.add_argument("--no-build", action="store_false", dest="build")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument(
        "--lto",
        action="store_true",
        help="with --configure: whole-program link-time optimization (not what the wheels use)",
    )
    parser.add_argument(
        "--suite",
        action="append",
        default=[],
        help="build and run only this suite, can be repeated",
    )
    parser.add_argument(
        "--engine",
        action="append",
        default=[],
        help="run only this engine, can be repeated",
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=5,
        help="runs of every measurement, each in a fresh process (default: 5)",
    )
    parser.add_argument(
        "--scale",
        type=float,
        default=1.0,
        help="multiply the iterations of every case, e.g. 0.1 for a quick check",
    )
    parser.add_argument(
        "--cpu",
        type=int,
        help="pin the benchmarks to this CPU (Linux)",
    )
    parser.add_argument(
        "--parity",
        type=float,
        default=5.0,
        help="percent around 1.00x that counts as on par (default: 5)",
    )
    parser.add_argument("--list", action="store_true", help="list the cases and exit")
    parser.add_argument(
        "--check",
        action="store_true",
        help="only check that the engines agree: one short run of every case, no report",
    )
    parser.add_argument("--save", type=Path, help="write the results to a JSON file")
    parser.add_argument(
        "--compare",
        type=Path,
        help="compare with the results of an earlier run (--save)",
    )
    return parser.parse_args()


def configure(args):
    subprocess.run(
        [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(args.build_dir),
            "-DCMAKE_BUILD_TYPE=Release",
            "-DSKBUILD_PROJECT_NAME=dspsim",
            "-DSKBUILD_PROJECT_VERSION=0.0.0",
            f"-DCMAKE_PREFIX_PATH={sysconfig.get_path('purelib')}",
            "-DBUILD_BENCHMARKS=ON",
            f"-DCMAKE_INTERPROCEDURAL_OPTIMIZATION={'ON' if args.lto else 'OFF'}",
        ],
        check=True,
    )


def build(args):
    targets = args.suite or ["benchmarks"]
    command = ["cmake", "--build", str(args.build_dir), "--config", "Release"]
    for target in targets:
        command += ["--target", target]
    subprocess.run(command, check=True)


def find_exe(build_dir: Path, suite: str) -> Path | None:
    """The suite's executable. Multi-config generators (Visual Studio) add a directory per configuration."""
    suffix = ".exe" if sys.platform == "win32" else ""
    for directory in (build_dir / "benchmarks", build_dir / "benchmarks" / "Release"):
        exe = directory / (suite + suffix)
        if exe.is_file():
            return exe
    return None


def discover(args) -> list[Case]:
    suites_file = args.build_dir / "benchmarks" / "suites.txt"
    if not suites_file.is_file():
        sys.exit(f"{suites_file} not found: run with --configure first.")
    cases = []
    for suite in suites_file.read_text().split():
        if args.suite and suite not in args.suite:
            continue
        exe = find_exe(args.build_dir, suite)
        if exe is None:
            sys.exit(f"Suite '{suite}' is not built in {args.build_dir}.")
        listing = subprocess.run(
            [str(exe), "--list", "--json"], check=True, capture_output=True, text=True
        )
        for line in listing.stdout.splitlines():
            if line.startswith("{"):
                entry = json.loads(line)
                cases.append(
                    Case(
                        suite=suite,
                        exe=exe,
                        name=entry["case"],
                        group=entry["group"],
                        kind=entry["kind"],
                        engines=entry["engines"],
                        iters=entry["iters"],
                        description=entry["description"],
                    )
                )
    return cases


def select(args, cases: list[Case]) -> list[Measurement]:
    patterns = [re.compile(p) for p in args.patterns]
    measurements = []
    for case in cases:
        if patterns and not any(p.search(case.path) for p in patterns):
            continue
        for engine in case.engines:
            if args.engine and engine not in args.engine:
                continue
            iters = max(1, round(case.iters * args.scale))
            measurements.append(Measurement(case, engine, iters))
    return measurements


def run_once(m: Measurement, cpu: int | None, repetition: int) -> None:
    command = [
        str(m.case.exe),
        "--case",
        m.case.name,
        "--engine",
        m.engine,
        "--iters",
        str(m.iters),
        "--layout",
        "x" * LAYOUTS[repetition % len(LAYOUTS)],
        "--json",
    ]
    if cpu is not None and hasattr(os, "sched_setaffinity"):
        command = ["taskset", "-c", str(cpu), *command]
    try:
        done = subprocess.run(
            command, check=False, capture_output=True, text=True, timeout=1800
        )
    except subprocess.TimeoutExpired:
        m.errors.append("timed out")
        return
    # The record is the last line that is a JSON object: SystemC may print messages of its own.
    records = [line for line in done.stdout.splitlines() if line.startswith("{")]
    if records:
        m.runs.append(json.loads(records[-1]))
    if done.returncode != 0:
        message = done.stderr.strip().splitlines()
        m.errors.append(message[-1] if message else f"exit status {done.returncode}")


def run_all(args, measurements: list[Measurement]) -> None:
    total = args.repeat * len(measurements)
    count = 0
    show_progress = sys.stderr.isatty()
    # Repetitions are the outer loop, so that a slow drift of the machine affects every case alike
    # and the runs of the engines of a case are next to each other.
    for repetition in range(args.repeat):
        for m in measurements:
            count += 1
            if show_progress:
                label = f"[{count}/{total}] {m.case.path} {m.engine}"
                print(f"\r{label:<78.78}", end="", file=sys.stderr, flush=True)
            # A measurement that failed once fails every time.
            if not m.errors:
                run_once(m, args.cpu, repetition)
        if not show_progress:
            print(f"repetition {repetition + 1}/{args.repeat} done", file=sys.stderr)
    if show_progress:
        print(f"\r{'':<78}\r", end="", file=sys.stderr, flush=True)


def metric_key(measurements: list[Measurement]) -> str:
    """The measure of time that every run has: cycles if the counters are available, else wall time."""
    runs = [run for m in measurements for run in m.runs]
    return "cycles" if runs and all(run["cycles"] > 0 for run in runs) else "wall_ns"


def check(measurements: list[Measurement]) -> list[str]:
    """Problems that make the results invalid: failed runs and engines that disagree."""
    problems = []
    by_case: dict[str, list[Measurement]] = {}
    for m in measurements:
        by_case.setdefault(m.case.path, []).append(m)
        for error in m.errors:
            problems.append(f"{m.case.path} [{m.engine}]: {error}")
        if len(m.checksums) > 1:
            problems.append(
                f"{m.case.path} [{m.engine}]: the checksum differs between runs: "
                f"{sorted(m.checksums)}"
            )
    for path, engines in by_case.items():
        engines = [m for m in engines if m.runs]
        checksums = {m.engine: sorted(m.checksums) for m in engines}
        if len({tuple(c) for c in checksums.values()}) > 1:
            problems.append(
                f"{path}: the engines disagree on the checksum: {checksums}"
            )
        sim_times = {m.engine: sorted(m.sim_times) for m in engines}
        if len({tuple(t) for t in sim_times.values()}) > 1:
            problems.append(
                f"{path}: the engines disagree on the simulated time: {sim_times}"
            )
    # The kinds of a group are the same design.
    by_group: dict[str, dict[str, set[str]]] = {}
    for m in measurements:
        group = f"{m.case.suite}/{m.case.group}"
        by_group.setdefault(group, {}).setdefault(m.case.kind, set()).update(
            m.checksums
        )
    for group, kinds in by_group.items():
        if len({tuple(sorted(c)) for c in kinds.values() if c}) > 1:
            printable = {kind: sorted(c) for kind, c in kinds.items()}
            problems.append(f"{group}: the kinds disagree on the checksum: {printable}")
    return problems


def format_count(value: float) -> str:
    if value >= 100:
        return f"{value:,.0f}"
    return f"{value:.1f}"


def print_table(rows: list[list[str]], header: list[str], left: int = 1) -> None:
    """Print rows in aligned columns. The first `left` columns are left aligned, the rest right aligned."""
    widths = [max(len(row[i]) for row in [header, *rows]) for i in range(len(header))]

    def line(cells):
        parts = [
            cell.ljust(width) if i < left else cell.rjust(width)
            for i, (cell, width) in enumerate(zip(cells, widths))
        ]
        return "  ".join(parts).rstrip()

    print(line(header))
    print("  ".join("-" * width for width in widths))
    for row in rows:
        print(line(row))


def report_engines(args, measurements: list[Measurement], key: str) -> None:
    """One row per case: every engine's cost per iteration and the ratio to the reference engine."""
    unit = "cycles" if key == "cycles" else "ns"
    engines = list(dict.fromkeys(m.engine for m in measurements))
    by_case: dict[str, dict[str, Measurement]] = {}
    for m in measurements:
        if m.runs:
            by_case.setdefault(m.case.path, {})[m.engine] = m

    compare = REFERENCE_ENGINE in engines and len(engines) > 1
    others = [e for e in engines if e != REFERENCE_ENGINE]
    # Wall time is shown for reference next to the cycles. Without a cycle counter it is the measure itself.
    show_wall = key != "wall_ns"
    header = ["case"]
    for engine in engines:
        header += [f"{engine} {unit}/it", "spread", "instr/it"]
        if show_wall:
            header.append("ns/it")
    if compare:
        for engine in others:
            header += [f"{engine}/{REFERENCE_ENGINE}", ""]

    rows = []
    kernel_rows = []
    band = args.parity / 100
    for path, by_engine in by_case.items():
        row = [path]
        for engine in engines:
            m = by_engine.get(engine)
            if m is None:
                row += ["", "", ""] + ([""] if show_wall else [])
                continue
            instructions = m.median("instructions")
            row += [
                format_count(m.median(key)),
                f"{m.spread(key) * 100:.1f}%",
                format_count(instructions) if instructions else "",
            ]
            if show_wall:
                row.append(format_count(m.median("wall_ns")))
        if compare:
            reference = by_engine.get(REFERENCE_ENGINE)
            for engine in others:
                m = by_engine.get(engine)
                if m is None or reference is None or not reference.median(key):
                    row += ["", ""]
                    continue
                ratio = m.median(key) / reference.median(key)
                verdict = (
                    "faster"
                    if ratio < 1 - band
                    else "SLOWER"
                    if ratio > 1 + band
                    else "on par"
                )
                # Only the perf_event counters leave the kernel out.
                in_kernel = (
                    m.counters == reference.counters == "perf_event"
                    and max(m.kernel_share, reference.kernel_share) > KERNEL_SHARE
                )
                row += [f"{ratio:.2f}x", verdict + (" *" if in_kernel else "")]
                if in_kernel:
                    kernel_rows.append(
                        [
                            path,
                            f"{m.kernel_share * 100:.0f}%",
                            f"{reference.kernel_share * 100:.0f}%",
                            f"{m.median('cpu_ns') / reference.median('cpu_ns'):.2f}x",
                        ]
                    )
        rows.append(row)

    print()
    print_table(rows, header)
    print()
    print(
        f"{unit}/it: median {unit} per iteration over {args.repeat} runs. "
        "spread: (max - min) / median of those runs."
    )
    if show_wall:
        print(
            "ns/it: median wall time per iteration, for reference. It includes whatever else "
            "the machine did."
        )
    if compare:
        print(
            f"engine/{REFERENCE_ENGINE}: ratio of the medians, below 1.00x is faster than "
            f"{REFERENCE_ENGINE}; within {args.parity:g}% is on par."
        )
    if kernel_rows:
        engine = others[0]
        print()
        print(
            "* The counters only count user mode, and these cases spend part of their CPU time in "
            "the kernel\n  (memory mapped for thread stacks, page faults). By CPU time, kernel "
            "included, the ratio is:"
        )
        print()
        print_table(
            kernel_rows,
            [
                "case",
                f"{engine} in kernel",
                f"{REFERENCE_ENGINE} in kernel",
                f"{engine}/{REFERENCE_ENGINE} by CPU time",
            ],
        )


def report_kinds(measurements: list[Measurement], key: str) -> None:
    """For every design that comes in several kinds: what each kind costs relative to the reference kind."""
    engines = list(dict.fromkeys(m.engine for m in measurements))
    groups: dict[str, dict[str, dict[str, Measurement]]] = {}
    for m in measurements:
        if m.runs:
            group = f"{m.case.suite}/{m.case.group}"
            groups.setdefault(group, {}).setdefault(m.case.kind, {})[m.engine] = m

    rows = []
    for group, kinds in groups.items():
        if len(kinds) < 2:
            continue
        reference_kind = (
            REFERENCE_KIND if REFERENCE_KIND in kinds else next(iter(kinds))
        )
        for kind, by_engine in kinds.items():
            if kind == reference_kind:
                continue
            row = [group, f"{kind} / {reference_kind}"]
            for engine in engines:
                m = by_engine.get(engine)
                reference = kinds[reference_kind].get(engine)
                if m is None or reference is None or not reference.median(key):
                    row.append("")
                else:
                    row.append(f"{m.median(key) / reference.median(key):.2f}x")
            rows.append(row)
    if rows:
        print()
        print(
            "Cost of the process kinds of each design, relative to its method kind "
            "(or its first kind, if it has no method kind):"
        )
        print()
        print_table(rows, ["design", "kind", *engines], left=2)


def percent_change(new: float, old: float | None) -> str:
    if not new or not old:
        return ""
    return f"{(new / old - 1) * 100:+.2f}%"


def report_baseline(args, measurements: list[Measurement], key: str) -> None:
    baseline = json.loads(args.compare.read_text())
    previous = {(r["suite"], r["case"], r["engine"]): r for r in baseline["results"]}
    rows = []
    for m in measurements:
        old = previous.get((m.case.suite, m.case.name, m.engine))
        if old is None or not m.runs:
            continue

        note = "" if sorted(m.checksums) == old["checksum"] else "checksum changed"
        rows.append(
            [
                m.case.path,
                m.engine,
                percent_change(m.median("instructions"), old.get("instructions")),
                percent_change(m.median(key), old.get(key)),
                f"{max(m.spread(key), old.get(key + '_spread', 0.0)) * 100:.1f}%",
                note,
            ]
        )
    print()
    described = baseline.get("meta", {}).get("git", "")
    print(f"Change against {args.compare}{f' ({described})' if described else ''}:")
    print()
    unit = "cycles" if key == "cycles" else "ns"
    print_table(
        rows, ["case", "engine", "instr/it", f"{unit}/it", "spread", ""], left=2
    )
    print()
    print(
        "instr/it is exact: any change is a change of the code that ran. "
        f"A change of {unit}/it within the spread is noise."
    )


def cmake_cache(build_dir: Path) -> dict[str, str]:
    cache = {}
    path = build_dir / "CMakeCache.txt"
    if path.is_file():
        for line in path.read_text().splitlines():
            match = re.match(r"^([A-Za-z0-9_]+):[A-Z]+=(.*)$", line)
            if match:
                cache[match.group(1)] = match.group(2)
    return cache


def metadata(args) -> dict:
    def git(*command: str) -> str:
        done = subprocess.run(
            ["git", *command], check=False, capture_output=True, text=True, cwd=ROOT
        )
        return done.stdout.strip()

    cache = cmake_cache(args.build_dir)
    cpu = platform.processor()
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.is_file():
        models = re.findall(r"model name\s*:\s*(.*)", cpuinfo.read_text())
        cpu = models[0] if models else cpu
    return {
        "git": git("describe", "--always", "--dirty"),
        "branch": git("rev-parse", "--abbrev-ref", "HEAD"),
        "cpu": cpu,
        "platform": platform.platform(),
        "compiler": cache.get("CMAKE_CXX_COMPILER", ""),
        "build_type": cache.get("CMAKE_BUILD_TYPE", ""),
        "lto": cache.get("CMAKE_INTERPROCEDURAL_OPTIMIZATION", "OFF"),
        "unity_build": cache.get("DSPSIM_UNITY_BUILD", "ON"),
        "repeat": args.repeat,
        "scale": args.scale,
    }


def main() -> int:
    args = parse_args()
    if not args.build_dir.is_absolute():
        args.build_dir = ROOT / args.build_dir

    if args.configure:
        configure(args)
    if args.build:
        build(args)

    cases = discover(args)
    measurements = select(args, cases)
    if args.list:
        for case in cases:
            engines = ", ".join(case.engines)
            print(
                f"{case.path:<44} {case.iters:>8} iters  [{engines}]  {case.description}"
            )
        return 0
    if not measurements:
        sys.exit("No case matches.")

    if args.check:
        # A twentieth of the iterations is enough to compare checksums. The warm-up stays as it is, so
        # every design still reaches its steady state.
        args.repeat = 1
        for m in measurements:
            m.iters = max(1, m.iters // 20)

    run_all(args, measurements)

    if args.check:
        problems = check(measurements)
        for problem in problems:
            print(problem)
        cases = len({m.case.path for m in measurements})
        if problems:
            print(f"FAILED: {len(problems)} problems in {cases} cases.")
            return 1
        print(f"{cases} cases: the engines and kinds of every design agree.")
        return 0

    key = metric_key(measurements)
    meta = metadata(args)
    print(
        f"dspsim {meta['git']} on {meta['cpu']}; {meta['build_type']} build, "
        f"LTO {meta['lto']}; counters: {measurements[0].counters}"
    )
    if key != "cycles":
        print(
            "No cycle counter is available here: the comparison uses wall time, which is less "
            "precise. Use more repetitions (--repeat) and longer runs (--scale)."
        )
    if any(run.get("multiplexed") for m in measurements for run in m.runs):
        print(
            "The hardware counters were shared with another user: the counts are estimates."
        )

    report_engines(args, measurements, key)
    report_kinds(measurements, key)
    if args.compare:
        report_baseline(args, measurements, key)

    if args.save:
        results = [m.summary() for m in measurements if m.runs]
        args.save.write_text(
            json.dumps({"meta": meta, "results": results}, indent=1) + "\n"
        )
        print(f"\nSaved to {args.save}")

    problems = check(measurements)
    if problems:
        print()
        print("FAILED:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
