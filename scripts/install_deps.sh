#!/usr/bin/env bash
# Script to install all dependencies into the deps folder. Run this from the root of the project.
mkdir -p ./deps
CMAKE_C_COMPILER="gcc"
CMAKE_CXX_COMPILER="g++"
# CMAKE_C_COMPILER="clang"
# CMAKE_CXX_COMPILER="clang++"

SPDLOG_DIR="./deps/spdlog"
# spdlog
if [[ ! -d "${SPDLOG_DIR}" ]]; then
    git clone https://github.com/gabime/spdlog.git ./deps/spdlog --branch v1.17.0 --depth 1
    cmake -S ./deps/spdlog -B ./deps/spdlog/build -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER}" -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER}" -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_BUILD_TYPE=Release -DSPDLOG_BUILD_PIC=ON -DSPDLOG_BUILD_SHARED=OFF
    cmake --build ./deps/spdlog/build --config Release
    cmake --install ./deps/spdlog/build --prefix ./deps/install
else
    echo "spdlog already exists at ${SPDLOG_DIR}"
fi
