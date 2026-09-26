# Script to install all dependencies into the deps folder. Run this from the root of the project.
$ErrorActionPreference = 'Stop'

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [string[]]$Arguments = @()
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE"
    }
}

New-Item -ItemType Directory -Force -Path ./deps | Out-Null

$DEPS_DIR = './.deps'
$SPDLOG_DIR = "${DEPS_DIR}/spdlog"

# spdlog
if (-not (Test-Path -Path $SPDLOG_DIR -PathType Container)) {
    Invoke-CheckedCommand git @(
        'clone',
        'https://github.com/gabime/spdlog.git',
        "${DEPS_DIR}/spdlog",
        '--branch', 'v1.17.0',
        '--depth', '1'
    )
    Invoke-CheckedCommand cmake @(
        '-S', "${DEPS_DIR}/spdlog",
        '-B', "${DEPS_DIR}/spdlog/build",
        '-DCMAKE_CXX_STANDARD=23',
        '-DCMAKE_CXX_STANDARD_REQUIRED=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DSPDLOG_BUILD_PIC=ON',
        '-DSPDLOG_BUILD_SHARED=OFF'
    )
    Invoke-CheckedCommand cmake @('--build', "${DEPS_DIR}/spdlog/build", '--config', 'Release')
    Invoke-CheckedCommand cmake @('--install', "${DEPS_DIR}/spdlog/build", '--prefix', "${DEPS_DIR}/install")
}
else {
    Write-Output "spdlog already exists at $SPDLOG_DIR"
}