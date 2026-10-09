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
uv run scripts/cpptest.py --configure          # configure + build + run, Debug, build dir ./build
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
- **`Context`** owns a simulation: registered models, the eval/update delta-cycle stacks, and a time-ordered event queue. A global `ContextFactory` tracks the active context; models self-register with it on construction. `elaborate()` finalizes all models (resolving lazy port binds), locks the design, and detaches the context from the global factory so a new independent context can be built. `Context::create()` throws `ContextConstructionError` while the active context is unelaborated (`Context::reset()` discards it); initial evaluation is scheduled on the first `run()`; `eval()` runs delta cycles until settled; `run(t)` advances simulated time by jumping event-to-event.
- **Two-phase delta cycle**: `eval()` computes; `Signal::write()` only sets the pending value `_d`. `update()` commits `_d → _q` and notifies subscribers (Changed/Posedge/Negedge), which schedules more work. All evals in a round see a consistent snapshot.
- **`Signal<T>`**, **`Input<T>`/`Output<T>`** ports (bind to a signal, or hierarchically to another port; port-to-port binds are recorded and resolved recursively at `finalize()`, so bind order doesn't matter), **`Clock`** (self-rescheduling signal).
- **`Module`** + **`ModuleName`**: `ModuleName` is an RAII temporary that pushes the module name onto the context's active-module stack and pops it after the member-initializer list runs, so submodules/ports declared as members get hierarchical names. Subclasses take `ModuleName` **by value** (see `DSPSIM_MODULE` / `DSPSIM_CTOR` macros in `module.h`).
- **Processes** (`process.h`, `coro.h`): modules register processes with static sensitivity (`always(...)`) and dynamic sensitivity (`next_trigger(...)`), or C++20 coroutines that `co_await wait(...)` on time deltas or sensitivity events. The old `SensitivityList`/`always << x` mechanism is retired (`.archive/`).
- **`vmodule/`**: wraps a Verilator-generated class as a `dspsim::Module` (`VPort`s map Verilator ports), with optional VCD/FST tracing.
- Hot path (the run loop) is performance-critical; elaboration-time code should favor a clean interface instead.

### Python bindings
- `src/_framework.cpp` defines the module by calling `bindings::bind_*` helpers from `src/include/bindings/*.h`. Templates are instantiated per type with suffixed names: `SignalU8..U64`, `SignalS8..S64`, `SignalFloat`, `SignalArray*`, `Input*/Output*`, `Dff*`, etc. Adding a type/class means updating both the binding header and the registration in `_framework.cpp`, and usually the re-export list in `src/dspsim/framework.py`.
- `src/dspsim/framework.py` is the public Python API: re-exports `_framework` and subclasses `Context`/`Module`. Creating a Python `Context` takes a global construction lock until it is elaborated or released: other threads block, and the same thread raises `ContextConstructionError`. Also adds Python-side async tasks via `add_task`, `await self.wait(...)`, backed by `PyTask`.
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

`docs/Architecture.md` is partly stale: it references old paths (`src/dspsim/framework/include/...`), the removed `SensitivityList`, and `scripts/cpptest.sh`. Trust the code over it. `docs/Coro.md` covers the coroutine awaitable design.
