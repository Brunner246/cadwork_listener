#Requires -Version 5.1
<#
.SYNOPSIS
  Reliable configure / build / test entry point for agents and humans.

.DESCRIPTION
  Always loads the MSVC x64 toolchain into the process, then runs CMake presets.
  Prefer this script over bare `cmake --build` in agent shells — bare builds
  often fail with missing standard headers when INCLUDE is empty.

.EXAMPLE
  # Full default: configure (if needed) + build local-relwithdebinfo
  .\scripts\syncro-build.ps1

.EXAMPLE
  # Build unit tests target and run ctest
  .\scripts\syncro-build.ps1 -Target listener_unit_tests -Test

.EXAMPLE
  # Force reconfigure + full suite
  .\scripts\syncro-build.ps1 -Configure -Test
#>
[CmdletBinding()]
param(
    # CMake preset name (matches CMakeUserPresets.json local-* presets).
    [ValidateSet('local-relwithdebinfo', 'local-debug', 'local-release',
                 'relwithdebinfo', 'debug', 'release')]
    [string]$Preset = 'local-relwithdebinfo',

    # Run cmake --preset (configure). Also runs automatically if build tree is missing.
    [switch]$Configure,

    # Optional single CMake target (e.g. listener_unit_tests, cadwork_listener).
    [string]$Target = '',

    # Run ctest after a successful build.
    [switch]$Test,

    # ctest -R filter (implies -Test).
    [string]$TestFilter = '',

    # Extra args passed to cmake --build (e.g. --parallel 8).
    [string[]]$BuildArgs = @(),

    # Skip MSVC env import (only if you already ran Import-VsDevEnvironment.ps1).
    [switch]$SkipVsEnv,

    # Repo root; default = parent of scripts/
    [string]$RepoRoot = ''
)

$ErrorActionPreference = 'Stop'

function Get-RepoRoot {
    if ($RepoRoot) { return (Resolve-Path $RepoRoot).Path }
    $here = $PSScriptRoot
    if (-not $here) { $here = Split-Path -Parent $MyInvocation.MyCommand.Path }
    return (Resolve-Path (Join-Path $here '..')).Path
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][scriptblock]$Action
    )
    Write-Host ""
    Write-Host "=== $Label ===" -ForegroundColor Cyan
    & $Action
    if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        throw "$Label failed with exit code $LASTEXITCODE"
    }
}

$root = Get-RepoRoot
Set-Location $root
Write-Host "Repo root: $root"
Write-Host "Preset:    $Preset"

# --- MSVC toolchain (critical for agent shells) ---
if (-not $SkipVsEnv) {
    $importScript = Join-Path $root 'scripts\Import-VsDevEnvironment.ps1'
    if (-not (Test-Path $importScript)) {
        throw "Missing $importScript"
    }
    . $importScript
}

# --- Configure ---
# CMakePresets binaryDir is ${sourceDir}/cmake-build-${presetName}
$binaryDir = Join-Path $root "cmake-build-$Preset"
$needConfigure = [bool]$Configure -or -not (Test-Path (Join-Path $binaryDir 'CMakeCache.txt'))

if ($needConfigure) {
    Invoke-Checked "cmake --preset $Preset" {
        & cmake --preset $Preset
    }
} else {
    Write-Host "Configure skipped (build tree present: $binaryDir). Pass -Configure to re-run."
}

# --- Build ---
$buildCmd = @('--build', '--preset', $Preset)
if ($Target) {
    $buildCmd += @('--target', $Target)
}
if ($BuildArgs -and $BuildArgs.Count -gt 0) {
    $buildCmd += $BuildArgs
}

Invoke-Checked "cmake $($buildCmd -join ' ')" {
    & cmake @buildCmd
}

# --- Test ---
$runTests = $Test -or ($TestFilter -and $TestFilter.Length -gt 0)
if ($runTests) {
    $ctestArgs = @('--test-dir', $binaryDir, '--output-on-failure')
    if ($TestFilter) {
        $ctestArgs += @('-R', $TestFilter)
    }

    Invoke-Checked "ctest $($ctestArgs -join ' ')" {
        & ctest @ctestArgs
    }
}

Write-Host ""
Write-Host "OK: syncro-build finished (preset=$Preset$(if ($Target) { ", target=$Target" })$(if ($runTests) { ", tests=yes" }))." -ForegroundColor Green
exit 0
