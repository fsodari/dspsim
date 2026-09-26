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

# SystemC
$SYSTEMC_DIR = './deps/systemc'
$CATCH2_DIR = './deps/Catch2'

if (-not (Test-Path -Path $SYSTEMC_DIR -PathType Container)) {
    Invoke-CheckedCommand git @(
        'clone',
        'https://github.com/accellera-official/systemc.git',
        './deps/systemc',
        '--branch', '3.0.2',
        '--depth', '1'
    )
    Invoke-CheckedCommand cmake @(
        '-S', './deps/systemc',
        '-B', './deps/systemc/build',
        '-DCMAKE_CXX_STANDARD=23',
        '-DCMAKE_CXX_STANDARD_REQUIRED=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DDISABLE_COPYRIGHT_MESSAGE=ON',
        '-DBUILD_SHARED_LIBS=OFF'
    )
    Invoke-CheckedCommand cmake @('--build', './deps/systemc/build', '--config', 'Release')
    Invoke-CheckedCommand cmake @('--install', './deps/systemc/build', '--prefix', './deps/install')
}
else {
    Write-Output "SystemC already exists at $SYSTEMC_DIR"
}

# Catch2
if (-not (Test-Path -Path $CATCH2_DIR -PathType Container)) {
    Invoke-CheckedCommand git @(
        'clone',
        'https://github.com/catchorg/Catch2.git',
        './deps/Catch2',
        '--branch', 'v3.16.0',
        '--depth', '1'
    )
    Invoke-CheckedCommand cmake @(
        '-S', './deps/Catch2',
        '-B', './deps/Catch2/build',
        '-DCMAKE_CXX_STANDARD=23',
        '-DCMAKE_CXX_STANDARD_REQUIRED=ON',
        '-DCMAKE_BUILD_TYPE=Release'
    )
    Invoke-CheckedCommand cmake @('--build', './deps/Catch2/build', '--config', 'Release')
    Invoke-CheckedCommand cmake @('--install', './deps/Catch2/build', '--prefix', './deps/install')
}
else {
    Write-Output "Catch2 already exists at $CATCH2_DIR"
}
