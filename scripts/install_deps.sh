#!/usr/bin/env bash
# Script to install all dependencies into the deps folder. Run this from the root of the project.
DEPS_DIR="./.deps"
mkdir -p "${DEPS_DIR}"

SPDLOG_DIR="${DEPS_DIR}/spdlog"
# spdlog
if [[ ! -d "${SPDLOG_DIR}" ]]; then
    git clone https://github.com/gabime/spdlog.git "${DEPS_DIR}/spdlog" --branch v1.17.0 --depth 1
    cmake -S "${DEPS_DIR}/spdlog" -B "${DEPS_DIR}/spdlog/build" -DCMAKE_CXX_STANDARD=23 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_BUILD_TYPE=Release -DSPDLOG_BUILD_PIC=ON -DSPDLOG_BUILD_SHARED=OFF
    cmake --build "${DEPS_DIR}/spdlog/build" --config Release
    cmake --install "${DEPS_DIR}/spdlog/build" --prefix "${DEPS_DIR}/install"
else
    echo "spdlog already exists at ${SPDLOG_DIR}"
fi
