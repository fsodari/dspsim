#!/usr/bin/env bash
# Script to install developer dependencies into the deps folder. Run this from the root of the project.

mkdir -p ./deps

# Install base dependencies.
./scripts/install_deps.sh

# systemc
SYSTEMC_DIR="./deps/systemc"
CATCH2_DIR="./deps/Catch2"

if [[ ! -d "${SYSTEMC_DIR}" ]]; then
    git clone https://github.com/accellera-official/systemc.git ./deps/systemc --branch 3.0.2 --depth 1
    cmake -S ./deps/systemc -B ./deps/systemc/build -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_BUILD_TYPE=Release -DDISABLE_COPYRIGHT_MESSAGE=ON -DBUILD_SHARED_LIBS=OFF
    cmake --build ./deps/systemc/build --config Release
    cmake --install ./deps/systemc/build --prefix ./deps/install
else
    echo "SystemC already exists at ${SYSTEMC_DIR}"
fi

# Catch2
if [[ ! -d "${CATCH2_DIR}" ]]; then
    git clone https://github.com/catchorg/Catch2.git ./deps/Catch2 --branch v3.16.0 --depth 1
    cmake -S ./deps/Catch2 -B ./deps/Catch2/build -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build ./deps/Catch2/build --config Release
    cmake --install ./deps/Catch2/build --prefix ./deps/install
else
    echo "Catch2 already exists at ${CATCH2_DIR}"
fi