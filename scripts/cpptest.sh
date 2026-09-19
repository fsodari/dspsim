#!/usr/bin/env bash
cmake --build ./build
# ctest --test-dir ./build/tests/cpp "$@"
./build/tests/cpp/tests "$@"