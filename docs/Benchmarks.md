# Benchmarks

The benchmarks in [benchmarks/](../benchmarks) compare dspsim against SystemC on the same designs, written
with equivalent constructs in both. They measure the simulation engine (scheduling, events, sensitivity,
time, coroutines), not the models, which do next to nothing.

```bash
uv run scripts/benchmarks.py --configure        # first run: configure, build, run everything
uv run scripts/benchmarks.py                    # rebuild and run everything
uv run scripts/benchmarks.py chain/dff          # only the cases matching a regular expression
uv run scripts/benchmarks.py --suite chain      # only build and run one suite
uv run scripts/benchmarks.py --check            # only check that the engines agree (about a second)
uv run scripts/benchmarks.py --save base.json   # keep the results ...
uv run scripts/benchmarks.py --compare base.json  # ... and compare a later run against them
```

SystemC is a developer dependency (`scripts/install_dev_deps.sh`).

## What is measured

A run measures **CPU cycles and instructions in user mode per iteration**, over the measured region only:

1. The design is constructed, elaborated and settled at time 0. dspsim does this in `elaborate()` and
   `run(0)`, SystemC in its first `sc_start()`. The time this takes is reported (`build`, `start`) and
   not part of the comparison.
2. A warm-up runs some iterations: a pipeline that is still filling does less work, and the first
   iterations take page faults and cache misses that later ones do not.
3. The counters run over the measured iterations, each of which changes the inputs, advances the
   simulation by one step and adds the outputs to a checksum.

Both numbers come from hardware counters read inside the benchmark (`perf_event_open` on Linux), so they
are not affected by other processes, interrupts or the time the process spends descheduled the way a
wall clock is. That is what makes short runs possible: the whole set runs in about a minute.

- **Instructions** are exact and repeat to the last digit from run to run. They are the number to use for
  the question "did this change make dspsim do more work": `--save` a baseline, make the change,
  `--compare`. Any difference is a difference in the code that ran.
- **Cycles** are the time. Instructions are not comparable between engines (dspsim and SystemC run at a
  different number of instructions per cycle), so the engine comparison uses cycles.
- Cycles depend on the **memory layout**: where the objects of a design happen to land relative to cache
  lines, to pages and to each other. The same binary runs 2 to 8% faster or slower with another layout,
  and over 10% in single cases, in SystemC as much as in dspsim. The layout is the same in every launch of
  the same command, so repeating a run does not average it out, and neither does a longer run: five
  identical launches agree to 1% on a number that is off by more. Anything moves the layout: a
  different build, another length of the command line (the arguments are on the stack, and SystemC copies
  them to the heap), another allocation before the design is built.
  So every measurement is repeated in fresh processes (`--repeat`, 5 by default), each with a **different
  layout** (the `--layout` argument of the executables, a text whose length is all that matters), the
  report shows the **median**, and `spread` shows the range of the runs relative to it. A spread of 10%
  means the case is sensitive to layout, and a difference smaller than the spread is noise.
- The wall time per iteration (`ns/it`) is shown for reference. It includes whatever else the machine did.

The counters stop at the kernel. That is what keeps interrupts out of the numbers, and it also leaves
out work that an engine makes the kernel do: SystemC maps and unmaps a stack for every thread it
creates, so the cases that create processes (`tasks/spawn`, `tasks/run_until`) cost it two to three
times what its cycle count says. The report marks those cases with `*` and adds the ratio by CPU time,
kernel included.

Where no hardware counters are available (macOS, a virtual machine without a virtual PMU, a container
that forbids `perf_event_open`), the comparison falls back to wall time, and to a cycle count from
`QueryThreadCycleTime` on Windows. Use more repetitions and longer runs there (`--repeat`, `--scale`).

Run it on an idle machine. `--cpu N` pins the benchmarks to one CPU. `BENCH_COUNTERS=time` in the
environment turns the hardware counters off, to see what a machine without them reports.

## The checks

The driver fails, with a non-zero exit status, when

- the engines of a case disagree on the checksum or on the simulated time,
- the kinds of a design disagree on the checksum,
- a checksum differs between two runs of the same thing,
- the checksummed output never changed (the design did nothing, or the run was shorter than its latency).

The checksum is a 64-bit hash of every sampled output value in order, so it also catches values that
arrive a cycle early or late. The stimulus is a fixed generator (splitmix64), not `<random>`, whose
distributions differ between standard libraries: the stimulus is the same on every platform.

## Reading the report

```
case                    dspsim cycles/it  spread  instr/it  ns/it  systemc cycles/it  spread  instr/it  ns/it  dspsim/systemc
chain/dff/n=500/method            22,827    1.8%    44,797  5,060             32,102    4.9%    78,477  7,230           0.71x  faster
```

`dspsim/systemc` is the ratio of the median cycles: below 1.00x dspsim is faster. Within 5% (`--parity`)
counts as on par, which is the target (see "Performance target" in `CLAUDE.md`).

A second table compares the **process kinds** of each design within one engine, relative to the method
kind. It answers what a coroutine or a dynamic wait costs compared to a method with a sensitivity list:

| kind | dspsim | SystemC |
| --- | --- | --- |
| `method` | `DSPSIM_METHOD` with `always(...)` | `SC_METHOD` with `sensitive << ...` |
| `method_dynamic` | method that calls `next_trigger(event)` | `SC_METHOD` with `next_trigger(event)` |
| `coro` | `DSPSIM_CORO` with `always(...)`, `co_await wait()` | `SC_THREAD` with `sensitive << ...`, `wait()` |
| `coro_dynamic` | coroutine that does `co_await wait(event)` | `SC_THREAD` with `wait(event)` |

Suites that are about a coroutine feature (sub-tasks, timeouts) name their kinds after what they compare.

## The suites

Each suite is one file in `benchmarks/` and one executable. The comment at the top of the file describes
its designs, and `scripts/benchmarks.py --list` lists every case.

| suite | designs | what they exercise |
| --- | --- | --- |
| `chain` | `dff`: flip-flops in series on one clock. `wire`: wires in series. | A clock edge that makes every process runnable at once (many evaluations in one delta cycle, one event with many subscribers); a change that ripples through many delta cycles with one evaluation each. `dff/n=2,step=7` is a tiny design where entering and leaving `run()` dominates. |
| `wide` | `wide`: one process writes 10000 signals. `matrix`: 16 stages of 15 signals. | The update phase: many signal commits per evaluation. |
| `stream` | `pipeline`: a source, n register slices and a sink with a valid/ready handshake. | Register-transfer level code: clocked and combinational processes, submodules, arrays of signals, back-pressure that travels backwards through the delta cycles. |
| `events` | `ring`, `broadcast`, `handshake`, `fifo` | Events that the model raises itself (`SensitivityEvent::notify()`, `sc_event`): one subscriber per notification, many subscribers, request/response pairs, queues that block. `fifo` also compares three ways to write a blocking call in a coroutine: inline, as a `Task`, as an awaitable. |
| `sensitivity` | `fanin`, `anyof`, `tree`, `fanout` | Sensitivity lists: a process sensitive to many signals, a dynamic wait on any of n events, combinational logic that settles over several delta cycles, a signal with many readers. |
| `timing` | `timers`, `clocks`, `timeout`, `delay` | Simulated time: a time queue with many entries, several clocks, waits with a timeout that does or does not expire, propagation delays. |
| `tasks` | `subtask`, `bus`, `spawn`, `run_until` | Coroutine features: awaited sub-tasks against function calls on a thread's stack, a transaction-level bus master, processes created while the simulation runs, a testbench that drives the design with `run_until()`. |
| `structs` | `bus`: registers in series whose ports are a struct with a nested struct. `arrays`: the same with 16 elements of two fields per link. | Struct ports. The kinds are ways to declare the ports of a link (see below), not process kinds. |

### The kinds of the `structs` suite

A dspsim struct port (`PortStruct`) is a group of ports: every field has a port and a signal of its own,
and the struct only refers to them. So the kinds of a design in this suite are the same simulation, and
they measure what it costs to reach the ports through a struct:

| kind | dspsim | SystemC |
| --- | --- | --- |
| `ports` | a port per field, each bound on its own | a port per field |
| `struct` | a `PortStruct`, bound as a whole | one port with the struct as its value (`sc_in<BusValue>`) |
| `soa` | a `PortStruct` whose fields are port arrays | one port with a struct of arrays as its value |
| `aos` | a `PortStructArrayOf`, element by `flat(i)` | a vector of ports with the struct of an element as their value |
| `aos_view` | the same array, through the views by field (`input_array<T>("data")`) that the module keeps | |
| `aos_index` | the same array, every access written as `ports[{i}].field` | |

What to expect:

- In dspsim, `struct` and `soa` run the same instructions as `ports`, and `aos` and `aos_view` are within
  a few percent of it: a struct costs nothing while the simulation runs.
- `aos_index` costs more than twice as much. `ports[{i}]` takes a `dspsim::Index`, a `std::vector`, so
  every access allocates one. This is the cost of `[{...}]` on any array, not of structs; in a process,
  use `flat(i)`, a range-based `for`, or keep a reference to the element.
- The `ports` kinds are the like-for-like comparison with SystemC. In the other kinds SystemC has one
  signal per struct where dspsim has one per field, so it commits one value where dspsim commits several
  (`soa`: 1 against 32). dspsim has no signal with a struct as its value, and these rows show what that
  would be worth; they are not regressions.

## How it is put together

Each suite is one executable. One run of it measures one engine of one case and prints one record:

```bash
build/benchmarks/chain --list
build/benchmarks/chain --case dff/n=500/coro --engine dspsim              # readable
build/benchmarks/chain --case dff/n=500/coro --engine dspsim --json       # the record the driver reads
perf record build/benchmarks/chain --case dff/n=500/coro --engine dspsim --iters 200000
```

Every measurement is a process of its own because SystemC can only elaborate one design per process, and
because it keeps one measurement from changing the memory layout of the next.
[scripts/benchmarks.py](../scripts/benchmarks.py) builds the suites, runs the cases, repeats them, checks
the checksums and prints the report.

- [benchmarks/harness/bench.h](../benchmarks/harness/bench.h): `Suite` and `Case`, the stimulus, the
  checksum and `measure()`, the loop that every engine runs. It depends on neither engine.
- [benchmarks/harness/counters.h](../benchmarks/harness/counters.h): the counters. Standard library only.
- [benchmarks/harness/dspsim_tb.h](../benchmarks/harness/dspsim_tb.h),
  [systemc_tb.h](../benchmarks/harness/systemc_tb.h): testbench base classes, and the standard testbench
  for a design with a clock, an input and an output.
- [benchmarks/models/](../benchmarks/models): the designs, `name.h` for dspsim and `name_sc.h` for SystemC.
  The dspsim models do not include SystemC, so they can be bound to Python or built on their own.

### Adding a case

A testbench is a class with `start()`, `step(Stimulus &)` and `time()` (see `bench.h`). For a design with
the ports `clk`, `in` and `out`, `ClockedTb<Model, T>` and `ClockedTbSC<Model, T>` are that class, and a
case is a few lines in the suite's `sc_main`:

```cpp
suite.add("dff/n=500", "method", {.iters = 8'000, .warmup = 1'000})
    .dspsim<ClockedTb<Chain<DffMethod<T>, T>, T>>(500)
    .systemc<ClockedTbSC<ChainSC<DffMethodSC<T>, T>, T>>(500);
```

The arguments after the testbench type are passed to the model's constructor. Rules for a new case:

- The same design in both engines, with equivalent constructs, and the same result. The model code is
  trivial: the cost should be the engine's.
- The result must not depend on the order in which the processes of a delta cycle run. The engines use
  different orders, so a checksum mismatch usually means the design has a race.
- The output that is checksummed depends on the work the design does.
- Size the iterations for roughly 30 to 100 ms of measured time, and the warm-up to reach the steady state.
- A new suite is a new file in `benchmarks/` and an `add_benchmark(name)` line in
  [benchmarks/CMakeLists.txt](../benchmarks/CMakeLists.txt).

`ClockedTb` changes the input two time units after each rising clock edge, away from the edge, so that
the result does not depend on how an engine orders an input change and a clock edge at the same time.

### Other engines

A record is a line of JSON (`suite`, `case`, `engine`, `iters`, `checksum`, `cycles`, `instructions`,
...), and the driver only needs records. A testbench in another language or for another simulator (the
Python bindings, cocotb) can take part by printing the same record for the same case name: the stimulus
and the checksum are a few lines each to reimplement, and `counters.h` has no dependencies.
`scripts/cocotb_benchmark.py` is a separate, older comparison of dspsim's Python API against cocotb.
