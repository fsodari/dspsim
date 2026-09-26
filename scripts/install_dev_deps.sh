#!/usr/bin/env bash
# Script to install developer dependencies into the deps folder. Run this from the root of the project.
DEPS_DIR=./.deps
mkdir -p "${DEPS_DIR}"

# Install base dependencies.
./scripts/install_deps.sh

# systemc
SYSTEMC_DIR="${DEPS_DIR}/systemc"
CATCH2_DIR="${DEPS_DIR}/Catch2"

if [[ ! -d "${SYSTEMC_DIR}" ]]; then
    git clone https://github.com/accellera-official/systemc.git "${DEPS_DIR}/systemc" --branch 3.0.2 --depth 1
    cmake -S "${DEPS_DIR}/systemc" -B "${DEPS_DIR}/systemc/build" -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_BUILD_TYPE=Release -DDISABLE_COPYRIGHT_MESSAGE=ON -DBUILD_SHARED_LIBS=OFF
    cmake --build "${DEPS_DIR}/systemc/build" --config Release
    cmake --install "${DEPS_DIR}/systemc/build" --prefix "${DEPS_DIR}/install"
else
    echo "SystemC already exists at ${SYSTEMC_DIR}"
fi

# Catch2
if [[ ! -d "${CATCH2_DIR}" ]]; then
    git clone https://github.com/catchorg/Catch2.git "${DEPS_DIR}/Catch2" --branch v3.16.0 --depth 1
    cmake -S "${DEPS_DIR}/Catch2" -B "${DEPS_DIR}/Catch2/build" -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_BUILD_TYPE=Release
    cmake --build "${DEPS_DIR}/Catch2/build" --config Release
    cmake --install "${DEPS_DIR}/Catch2/build" --prefix "${DEPS_DIR}/install"
else
    echo "Catch2 already exists at ${CATCH2_DIR}"
fi