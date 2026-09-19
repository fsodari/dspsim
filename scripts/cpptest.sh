#!/usr/bin/env bash
cmake --build ./build --config Debug --target tests
# ctest --test-dir ./build/tests/cpp "$@"
./build/tests/cpp/tests "$@"