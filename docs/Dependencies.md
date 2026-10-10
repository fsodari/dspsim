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

## liblz4-dev
fst tracing on linux. Dev/user dependency. Main project doesn't need it to build.
Projects using dspsim will need it to build models that use fst tracing.

## lz4 (macOS)
fst tracing on macOS: `brew install lz4`. Verilated targets need `dspsim_target_fst_deps(target)` (the generated
CMake calls it) so that the Homebrew prefix (`/opt/homebrew` on Apple Silicon) is on the search path.
