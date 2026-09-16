#!/usr/bin/env pwsh
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

param(
  [Parameter(Position = 0)]
  [string]$Target,

  [string]$BuildDir,
  [string]$DistDir,
  [string]$BuildType = "Release",
  [string]$VcpkgRoot
)

$ScriptDir = Split-Path -Parent $PSCommandPath
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "../../..")
if (-not $BuildDir) {
  $BuildDir = Join-Path $RepoRoot "build/local-windows"
}
if (-not $DistDir) {
  $DistDir = Join-Path $RepoRoot "dist/local-windows"
}
if (-not $VcpkgRoot) {
  $VcpkgRoot = Join-Path $HOME ".openapoc/vcpkg"
}
$VcpkgInstallRoot = Join-Path $BuildDir "vcpkg_installed"

function Show-Usage {
  @"
usage: scripts/local/windows/run.ps1 <target> [options]

targets:
  deps        bootstrap vcpkg and install manifest dependencies
  minimal-cd  download the minimal CI cd.iso if data/cd.iso is missing
  configure   configure CMake
  build       build the configured tree
  test        run CTest
  package     create Windows package artifacts
  lint        run format-sources through the workflow lint helper
  all         minimal-cd, deps, configure, build, and test

options:
  -BuildDir PATH     build directory, defaults to build/local-windows
  -DistDir PATH      package output directory, defaults to dist/local-windows
  -BuildType TYPE    CMake build type, defaults to Release
  -VcpkgRoot PATH    vcpkg checkout, defaults to `$HOME\.openapoc\vcpkg
"@
}

function Require-Command([string]$Name, [string]$Hint) {
  if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
    throw "missing dependency: $Name. $Hint"
  }
}

function Check-Python {
  Require-Command python "Install Python 3 and make it available on PATH."
}

function Check-BuildDeps {
  Check-Python
  Require-Command cmake "Install CMake and make it available on PATH."
  Require-Command ctest "Install CMake, which provides ctest."
  Require-Command git "Install Git for Windows."
  Require-Command ninja "Install Ninja and make it available on PATH."
  Require-Command cl "Run this from a Visual Studio Developer PowerShell or load VsDevCmd first."
}

function Find-Nsis {
  $Command = Get-Command "makensis.exe" -ErrorAction SilentlyContinue
  if ($Command) {
    return $Command.Source
  }
  $Candidates = @(
    "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
    "$env:ProgramFiles\NSIS\makensis.exe"
  )
  foreach ($Candidate in $Candidates) {
    if ($Candidate -and (Test-Path $Candidate)) {
      return $Candidate
    }
  }
  return $null
}

function Check-Cd {
  if (-not (Test-Path "data/cd.iso")) {
    throw "data/cd.iso is missing. Add original game data or run: scripts/local/windows/run.ps1 minimal-cd"
  }
}

function Invoke-Deps {
  Check-BuildDeps
  $env:VCPKG_DEFAULT_TRIPLET = "x64-windows"
  python .github/scripts/package.py windows-vcpkg `
    --root $VcpkgRoot `
    --workspace $RepoRoot `
    --install-root $VcpkgInstallRoot
}

function Invoke-MinimalCd {
  Check-Python
  if (-not (Test-Path "data/cd.iso")) {
    $CdIso = Join-Path $RepoRoot "data/cd.iso"
    python .github/scripts/cmake.py minimal --output $CdIso
  }
}

function Invoke-Configure {
  Check-BuildDeps
  Check-Cd
  python .github/scripts/cmake.py mkdir --path $BuildDir
  python .github/scripts/cmake.py configure `
    --source $RepoRoot `
    --build-dir $BuildDir `
    --build-type $BuildType `
    --option=-DMSVC_PDB=ON `
    --option=-DUSE_PCH=ON `
    "--option=-DVCPKG_INSTALLED_DIR=$VcpkgInstallRoot" `
    "--option=-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake"
}

function Invoke-Build {
  Check-BuildDeps
  python .github/scripts/cmake.py build --build-dir $BuildDir --config $BuildType
}

function Invoke-Test {
  Check-BuildDeps
  python .github/scripts/cmake.py test --build-dir $BuildDir --config $BuildType
}

function Invoke-Package {
  Check-BuildDeps
  $Nsis = Find-Nsis
  $PackageArgs = @(
    ".github/scripts/package.py", "windows",
    "--workspace", $RepoRoot,
    "--build-dir", $BuildDir,
    "--dist-dir", $DistDir,
    "--vcpkg-root", $VcpkgRoot
  )
  if ($Nsis) {
    $PackageArgs += @("--nsis", $Nsis)
  } else {
    Write-Warning "NSIS not found; creating ZIP artifacts without installer."
  }
  python @PackageArgs
}

function Invoke-Lint {
  Check-BuildDeps
  python .github/scripts/lint.py format --build-dir $BuildDir --workspace $RepoRoot
}

if (-not $Target) {
  Show-Usage
  exit 2
}

Set-Location $RepoRoot

switch ($Target) {
  "deps" { Invoke-Deps }
  "minimal-cd" { Invoke-MinimalCd }
  "configure" { Invoke-Configure }
  "build" { Invoke-Build }
  "test" { Invoke-Test }
  "package" { Invoke-Package }
  "lint" { Invoke-Lint }
  "all" {
    Invoke-MinimalCd
    Invoke-Deps
    Invoke-Configure
    Invoke-Build
    Invoke-Test
  }
  { $_ -in @("-h", "--help", "help") } { Show-Usage }
  default {
    Show-Usage
    throw "unknown target: $Target"
  }
}
