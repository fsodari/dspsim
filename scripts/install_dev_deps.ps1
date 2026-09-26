# Script to install developer dependencies into the deps folder. Run this from the root of the project.
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

# Install base dependencies.
& ./scripts/install_deps.ps1

$DEPS_DIR = './.deps'
# SystemC
$SYSTEMC_DIR = "${DEPS_DIR}/systemc"
$CATCH2_DIR = "${DEPS_DIR}/Catch2"

if (-not (Test-Path -Path $SYSTEMC_DIR -PathType Container)) {
    Invoke-CheckedCommand git @(
        'clone',
        'https://github.com/accellera-official/systemc.git',
        "${DEPS_DIR}/systemc",
        '--branch', '3.0.2',
        '--depth', '1'
    )
    Invoke-CheckedCommand cmake @(
        '-S', "${DEPS_DIR}/systemc",
        '-B', "${DEPS_DIR}/systemc/build",
        '-DCMAKE_CXX_STANDARD=23',
        '-DCMAKE_CXX_STANDARD_REQUIRED=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DDISABLE_COPYRIGHT_MESSAGE=ON',
        '-DBUILD_SHARED_LIBS=OFF'
    )
    Invoke-CheckedCommand cmake @('--build', "${DEPS_DIR}/systemc/build", '--config', 'Release')
    Invoke-CheckedCommand cmake @('--install', "${DEPS_DIR}/systemc/build", '--prefix', "${DEPS_DIR}/install")
}
else {
    Write-Output "SystemC already exists at $SYSTEMC_DIR"
}

# Catch2
if (-not (Test-Path -Path $CATCH2_DIR -PathType Container)) {
    Invoke-CheckedCommand git @(
        'clone',
        'https://github.com/catchorg/Catch2.git',
        "${DEPS_DIR}/Catch2",
        '--branch', 'v3.16.0',
        '--depth', '1'
    )
    Invoke-CheckedCommand cmake @(
        '-S', "${DEPS_DIR}/Catch2",
        '-B', "${DEPS_DIR}/Catch2/build",
        '-DCMAKE_CXX_STANDARD=23',
        '-DCMAKE_CXX_STANDARD_REQUIRED=ON',
        '-DCMAKE_BUILD_TYPE=Release'
    )
    Invoke-CheckedCommand cmake @('--build', "${DEPS_DIR}/Catch2/build", '--config', 'Release')
    Invoke-CheckedCommand cmake @('--install', "${DEPS_DIR}/Catch2/build", '--prefix', "${DEPS_DIR}/install")
}
else {
    Write-Output "Catch2 already exists at $CATCH2_DIR"
}
