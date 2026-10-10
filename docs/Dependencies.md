# Project Dependencies

## spdlog
https://github.com/gabime/spdlog.git

Logging utility.

## systemc
https://github.com/accellera-official/systemc.git

Similar framework to dspsim. Used for benchmark comparisons.

## Catch2
https://github.com/catchorg/Catch2.git

Unit testing framework.

# System Dependencies

## lz4
fst tracing on linux and macOS. Dev/user dependency. Main project doesn't need it to build.
Projects using dspsim will need it to build models that use fst tracing.

- Debian/Ubuntu: `apt install liblz4-dev`
- Fedora/RHEL: `dnf install lz4-devel`
- macOS: `brew install lz4`

Verilated targets with fst tracing need `dspsim_target_fst_deps(target)` (the generated CMake calls it). It fails
the configure step with an install hint when lz4 is missing, and adds the lz4 search paths when they are not compiler
defaults, e.g. the Homebrew prefix on Apple Silicon (`/opt/homebrew`).
