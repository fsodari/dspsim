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

$SPDLOG_DIR = './deps/spdlog'

# spdlog
if (-not (Test-Path -Path $SPDLOG_DIR -PathType Container)) {
    Invoke-CheckedCommand git @(
        'clone',
        'https://github.com/gabime/spdlog.git',
        './deps/spdlog',
        '--branch', 'v1.17.0',
        '--depth', '1'
    )
    Invoke-CheckedCommand cmake @(
        '-S', './deps/spdlog',
        '-B', './deps/spdlog/build',
        '-DCMAKE_CXX_STANDARD=23',
        '-DCMAKE_CXX_STANDARD_REQUIRED=ON',
        '-DCMAKE_BUILD_TYPE=Release',
        '-DSPDLOG_BUILD_PIC=ON',
        '-DSPDLOG_BUILD_SHARED=OFF'
    )
    Invoke-CheckedCommand cmake @('--build', './deps/spdlog/build', '--config', 'Release')
    Invoke-CheckedCommand cmake @('--install', './deps/spdlog/build', '--prefix', './deps/install')
}
else {
    Write-Output "spdlog already exists at $SPDLOG_DIR"
}