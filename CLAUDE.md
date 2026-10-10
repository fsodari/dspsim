# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

dspsim is a C++23 discrete-event (delta-cycle) simulation engine for hardware designs, exposed to Python (>=3.14) via nanobind as `dspsim._framework`. Models are either hand-written C++/Python modules or Verilator-generated models of SystemVerilog RTL. Built with scikit-build-core + CMake, managed with `uv`.

## Commands

```bash
# Editable install of the Python package (rebuilds the C++ extension on import when sources change)
uv sync -v

# Python tests
uv run pytest tests
uv run pytest tests/test_signal.py::test_name

# C++ tests (Catch2, single `tests` executable). First run needs --configure.
uv run scripts/cpptest.py --configure          # configure + build + run, Release, build dir ./build
uv run scripts/cpptest.py                      # rebuild + run
uv run scripts/cpptest.py "[dff]"              # extra args are passed to the Catch2 binary (tag/name filter)
uv run scripts/cpptest.py --no-build "[coro]"  # run without rebuilding

# Lint / format Python
uv run ruff check . && uv run ruff format .
```

- `cpptest.py` passes `CMAKE_PREFIX_PATH` = the venv site-packages so CMake finds nanobind and verilator (from the `verilator-dspsim` wheel). Run it via the venv.
- On first configure, if `.deps/install` is missing, CMake runs `scripts/install_deps.sh` (or `install_dev_deps.sh` with `-DINSTALL_DEV_DEPS=ON`) to build spdlog / Catch2 / SystemC locally. Linux FST tracing needs `liblz4-dev`.
- Benchmarks (`benchmarks/`, some compare against SystemC) are `EXCLUDE_FROM_ALL` targets; `scripts/benchmarks.py` drives them.
- Wheels: `uvx cibuildwheel` (CI in `.github/workflows/build_wheels.yml`, publishes to PyPI on `v*` tags).

## Architecture

### C++ core (`src/*.cpp`, headers in `src/include/dspsim/`) → static lib `dspsim-core`
- **`Context`** owns a simulation: registered models, the eval/update delta-cycle stacks, and a time-ordered event queue. A global `ContextFactory` tracks the active context; models self-register with it on construction. `elaborate()` finalizes all models (resolving lazy port binds), locks the design, and releases the context from the global factory so a new independent context can be built. `Context::create()` takes a construction lock in `ContextFactory`: other threads block until the context is elaborated or `release()`d, and the same thread gets `ContextConstructionError` (`Context::reset()` discards it). `Context::obtain()` never creates a context; it throws unless the calling thread has one under construction, so models built outside `create()`…`elaborate()` fail; initial evaluation is scheduled on the first `run()`; `eval()` runs delta cycles until settled; `run(t)` advances simulated time by jumping event-to-event.
- **Two-phase delta cycle**: `eval()` computes; `Signal::write()` only sets the pending value `_d`. `update()` commits `_d → _q` and notifies subscribers (Changed/Posedge/Negedge), which schedules more work. All evals in a round see a consistent snapshot.
- **`Signal<T>`**, **`Input<T>`/`Output<T>`** ports (bind to a signal, or hierarchically to another port; port-to-port binds are recorded and resolved recursively at `finalize()`, so bind order doesn't matter), **`Clock`** (self-rescheduling signal).
- **`Module`** + **`ModuleName`**: `ModuleName` is an RAII temporary that pushes the module name onto the context's active-module stack and pops it after the member-initializer list runs, so submodules/ports declared as members get hierarchical names. Subclasses take `ModuleName` **by value** (see `DSPSIM_MODULE` / `DSPSIM_CTOR` macros in `module.h`). Copies share one construction scope that ends when the last copy dies, so a module derived from another module passes its `ModuleName` on by value. Python has no RAII: `Module.__init_subclass__` wraps each `__init__`, and the outermost call opens the scope (`with ModuleName(name)`), initializes the C++ `Module` once, runs the `__init__` chain, then calls `context.own_model()`. Python subclass `__init__` must take a `name` argument; `super().__init__(name)` is optional.
- **Bit slicing/packing** (`bits.h`, `bitsel.h`, `derived_signal.h`): `sig[{hi, lo}]` / `pack(a, b)` give an untyped, unsigned `BitSel` that can be read/written directly or bound to any integral `Input`/`Output` of the same width. Binding creates a context-owned `DerivedSignal<T>` that sources update in the same delta cycle (a second pass in `Context::_update_signals()`); output writes go through to the sources. See `docs/BitSlicing.md`.
- **Processes** (`process.h`, `coro.h`): modules register processes with static sensitivity (`always(...)`) and dynamic sensitivity (`next_trigger(...)`), or C++20 coroutines (`Task<>`) that `co_await wait(...)` on time deltas, sensitivity events, or events with a timeout (`wait(event, timeout)` returns a `WaitResult`). A dynamic wait is the only thing a suspended coroutine waits for: static sensitivity is paused and the losing side of event-or-timeout is dismissed (time events are tagged with the process's `wake_count`). `Task<T>` coroutines are composable (`co_await` one from another coroutine of the same process; leaf waits record the process's resume point), which is how modules expose async operations such as `AxisRx::receive(n, timeout)` / `AxisTx::send(data, timeout)`. From non-coroutine code, `Context::run_until(task, timeout)` runs the simulation until the task completes and returns its value (`TimeoutError` on timeout); `run_until(event, timeout)` for a plain event. See `docs/Coro.md`. The old `SensitivityList`/`always << x` mechanism is retired (`.archive/`).
- **`vmodule/`**: wraps a Verilator-generated class as a `dspsim::Module` (`VPort`s map Verilator ports), with optional VCD/FST tracing.
- Hot path (the run loop) is performance-critical; elaboration-time code should favor a clean interface instead. The engine is memory-bound (a deep DFF chain costs ~7 L1 misses per process evaluation), so layout matters: `SignalBase` keeps the fields that `write()`/`update()` touch last, next to `Signal<T>::d_/q_`; `ProcessBase` keeps its per-evaluation flags first; `SensitivityEvent` allocates its subscriber lists lazily so an unsubscribed event (most of them) is 24 bytes and `notify()` is a null check; `DSPSIM_METHOD` binds the member function at compile time (`register_method<&M::eval>(this)`, a `MethodProcess` with a plain function pointer, no `std::function`). `benchmarks/nested_dff_really_deep` is the reference for this path (`uv run scripts/benchmarks.py --configure --target nested_dff_really_deep`).
- **Performance target**: the benchmarks compare against SystemC, and being comparable to SystemC is good enough. When a benchmark is on par with or faster than SystemC, stop optimizing; only a clear regression against SystemC (or against `main`, measured with the same binary args on both) needs work.

### Python bindings
- `src/_framework.cpp` defines the module by calling `bindings::bind_*` helpers from `src/include/bindings/*.h`. Templates are instantiated per type with suffixed names: `SignalU8..U64`, `SignalS8..S64`, `SignalFloat`, `SignalArray*`, `Input*/Output*`, `Dff*`, etc. Adding a type/class means updating both the binding header and the registration in `_framework.cpp`, and usually the re-export list in `src/dspsim/framework.py`.
- `src/dspsim/framework.py` is the public Python API: re-exports `_framework` and subclasses `Context`/`Module`. The Python `Context` relies on the C++ construction lock; the `create` bindings release the GIL while waiting (GIL handling stays in the bindings, never in `dspsim-core`). Also adds Python-side async tasks via `add_task`, `await self.wait(...)`, backed by `PyTask`; `ctx.run_until(awaitable, timeout)` drives any awaitable to completion. C++ `Task<T>` methods are exposed to Python by wrapping them with `py_task(context, task)` in the binding, plus one `bind_task<T>` registration per result type (`Task`, `TaskBool`, `TaskList*`); `AxisRx*`/`AxisTx*` (`axis_bindings.h`) are the examples.
- Stubs (`_framework.pyi`) are generated at install time by `dspsim_add_stub`.
- All extensions use `NB_DOMAIN dspsim`, and separately built model extensions must call `dspsim.link_module(module)` to share the main global `ContextFactory`, or their models register with a different context.

### Verilator codegen (two paths, same Jinja templates in `src/dspsim/templates/`)
1. **Library build (CMake)**: `dspsim_add_module()` in `cmake/dspsim-utils.cmake` runs `python -m dspsim.generate --pyproject ... --output-dir ...`, which reads `[tool.dspsim]` from a pyproject (`project.py`: sources, include_dirs, parameters, trace), introspects each module with `verilator --json-only` (`verilator.py`, `module_info.py`), and emits nanobind wrapper C++ + a CMake include that verilates the sources. See `examples/some_example/`. Downstream projects find the installed package via `dspsim --cmake_dir` / `dspsim --include_dir`.
2. **JIT build (Python)**: `dspsim.builder.vbuilder(source)` class decorator / `build_vmodule()` generates a per-model CMake project, builds it under `.dspsim_cache/` (override with `DSPSIM_CACHE_DIR`), keyed by a hash of generated content, then imports it and calls `link_module`. Class annotations override parameters and optionally validate ports.

## Conventions (from docs/StyleGuide.md)

- C++: Allman braces, 4-space indent, braces required even on one-line bodies. PascalCase classes, snake_case functions/variables, private members with a trailing underscore (older code still uses leading `_`).
- Class member order: types, constants, ctors/dtor, static factories, public, protected, private methods, then public, protected, private data. Members must be declared in initialization order (`-Werror=reorder` is on). A `Context*` member goes first.
- Prefer forward declarations over including headers of classes that reference each other. Put definitions in `.cpp` files, not inline in headers. Definitions follow declaration order.
- Doxygen docstrings on header declarations. Add tests for new features.
- `dspsim-core` builds with `-Wall -Wextra -Wpedantic -Werror`. New public headers must be added to the `FILE_SET HEADERS` list in `src/CMakeLists.txt`, and new sources to `add_library(dspsim-core ...)`.
- **C++ tests**: all `tests/cpp/*.cpp` link into one binary, so wrap file-local helper classes in an anonymous `namespace { }` to avoid silent ODR violations. New test files must be added to `tests/cpp/CMakeLists.txt`. A Catch2 listener (`test_listeners.cpp`) resets the global context after each test case. Verilated test models (`hdl/Skid.sv`, `NDArrayModel.sv`) are built there with `verilate(...)`.
- Python is formatted with ruff (preview format enabled).

## Docs caveat

`docs/Architecture.md` is partly stale: it references old paths (`src/dspsim/framework/include/...`), the removed `SensitivityList`, and `scripts/cpptest.sh`. Trust the code over it. `docs/Coro.md` covers the coroutine awaitable design (leaf waits, `Task<T>`, `run_until`, Python bridge). `docs/BitSlicing.md` covers slicing and packing.
