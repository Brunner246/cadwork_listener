#Requires -Version 5.1
<#
.SYNOPSIS
  Import MSVC x64 developer environment into the current PowerShell process.

.DESCRIPTION
  Agent shells and plain PowerShell often lack INCLUDE / LIB / PATH for cl.exe.
  That produces false failures such as:
    fatal error C1083: Cannot open include file: 'filesystem'
    fatal error C1083: Cannot open include file: 'cstddef'

  Call this before any cmake --build / ninja / cl on Syncro.
  Safe to call repeatedly; re-imports when INCLUDE looks empty or -Force is set.
#>
[CmdletBinding()]
param(
    [switch]$Force
)

function Test-MsvcEnvReady {
    if (-not $env:INCLUDE -or $env:INCLUDE.Trim().Length -eq 0) {
        return $false
    }
    # cl must resolve once vcvars has run
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    return $null -ne $cl
}

if (-not $Force -and (Test-MsvcEnvReady)) {
    Write-Verbose "MSVC x64 environment already present."
    return
}

$vswhereCandidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
)
$vswhere = $vswhereCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vswhere) {
    throw "vswhere.exe not found. Install Visual Studio 2022+ with C++ desktop workload."
}

$installPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath 2>$null
if (-not $installPath) {
    # Fallback: any VS with VC tools
    $installPath = & $vswhere -latest -products * -property installationPath 2>$null
}
if (-not $installPath -or -not (Test-Path $installPath)) {
    throw "No Visual Studio installation with VC tools found via vswhere."
}

$vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found at: $vcvars"
}

Write-Host "Importing MSVC x64 env from: $vcvars"

# Capture environment after vcvars (cmd.exe only understands the batch file).
$raw = & cmd.exe /c "`"$vcvars`" >nul 2>nul && set"
if ($LASTEXITCODE -ne 0 -and -not $raw) {
    throw "vcvars64.bat failed (exit $LASTEXITCODE)."
}

$count = 0
foreach ($line in $raw) {
    if ($line -match '^\s*([^=]+)=(.*)$') {
        $name = $Matches[1]
        $value = $Matches[2]
        # Skip pseudo vars that confuse PowerShell
        if ($name -eq '' -or $name -match '^\d') { continue }
        [System.Environment]::SetEnvironmentVariable($name, $value, 'Process')
        $count++
    }
}

if (-not (Test-MsvcEnvReady)) {
    throw @"
MSVC environment import ran but cl.exe / INCLUDE still unavailable.
vs install: $installPath
INCLUDE length: $((if ($env:INCLUDE) { $env:INCLUDE.Length } else { 0 }))
"@
}

Write-Host "MSVC x64 environment ready ($count variables; cl=$($(Get-Command cl.exe).Source))."
