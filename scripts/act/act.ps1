#!/usr/bin/env pwsh
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

param(
  [Parameter(Position = 0)]
  [string]$Command,

  [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
  [string[]]$ActArgs = @()
)

$ScriptDir = Split-Path -Parent $PSCommandPath
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "../..")
$ActBin = if ($env:ACT_BIN) { $env:ACT_BIN } else { "act" }

function Show-Usage {
  @"
usage: scripts/act/act.ps1 <command> [act args...]

commands:
  cmake          run .github/workflows/cmake.yml as push
  hygiene        run .github/workflows/hygiene.yml as pull_request
  lint           run .github/workflows/lint.yml as push
  linux-pack     run .github/workflows/linux-pack.yml as push
  macos-pack     run .github/workflows/macos-pack.yml as push
  windows-pack   run .github/workflows/windows-pack.yml as push
  all-series     run every workflow wrapper one after another
  all-parallel   run every workflow wrapper concurrently in this checkout
  dry-run NAME   validate a workflow without creating containers
  list           list all workflow jobs through act

environment:
  ACT_BIN                       act binary path, defaults to "act"
  ACT_LOG_DIR                   log directory for all-parallel, defaults to .act/logs
  ACT_PLATFORM                  act runner image mapping, defaults by workflow
  ACT_ALLOW_SHARED_WORKTREE=1   required for all-parallel
"@
}

function Require-Act {
  if (-not (Get-Command $ActBin -ErrorAction SilentlyContinue)) {
    throw "act not found. Install act or set ACT_BIN=/path/to/act."
  }
}

function Get-DefaultPlatform([string]$Workflow) {
  switch ($Workflow) {
    "macos-pack.yml" { "macos-15=catthehacker/ubuntu:act-24.04" }
    default { "ubuntu-24.04=catthehacker/ubuntu:act-24.04" }
  }
}

function Invoke-Workflow([string]$EventName, [string]$Workflow, [string[]]$ExtraArgs) {
  Require-Act
  Push-Location $RepoRoot
  try {
    $Platform = if ($env:ACT_PLATFORM) { $env:ACT_PLATFORM } else { Get-DefaultPlatform $Workflow }
    if ($Platform) {
      & $ActBin $EventName -P $Platform -W ".github/workflows/$Workflow" @ExtraArgs
    } else {
      & $ActBin $EventName -W ".github/workflows/$Workflow" @ExtraArgs
    }
    if ($LASTEXITCODE -ne 0) {
      exit $LASTEXITCODE
    }
  } finally {
    Pop-Location
  }
}

function Invoke-Named([string]$Name, [string[]]$ExtraArgs) {
  switch ($Name) {
    "cmake" { Invoke-Workflow "push" "cmake.yml" $ExtraArgs }
    "hygiene" { Invoke-Workflow "pull_request" "hygiene.yml" $ExtraArgs }
    "lint" { Invoke-Workflow "push" "lint.yml" $ExtraArgs }
    "linux-pack" { Invoke-Workflow "push" "linux-pack.yml" $ExtraArgs }
    "macos-pack" { Invoke-Workflow "push" "macos-pack.yml" $ExtraArgs }
    "windows-pack" { Invoke-Workflow "push" "windows-pack.yml" $ExtraArgs }
    default {
      Show-Usage
      throw "unknown workflow command: $Name"
    }
  }
}

function Invoke-Series([string[]]$ExtraArgs) {
  foreach ($Name in @("cmake", "hygiene", "lint", "linux-pack", "macos-pack", "windows-pack")) {
    Invoke-Named $Name $ExtraArgs
  }
}

function Invoke-Parallel([string[]]$ExtraArgs) {
  if ($env:ACT_ALLOW_SHARED_WORKTREE -ne "1") {
    @"
Refusing to run all workflows in parallel against one shared checkout.

GitHub-hosted workflows run in isolated workspaces. Local act runs share this
checkout by default, so parallel runs can race on files such as dist/ and
workflow artifacts.

Set ACT_ALLOW_SHARED_WORKTREE=1 if you explicitly accept that risk.
"@ | Write-Error
  }

  $LogDir = if ($env:ACT_LOG_DIR) { $env:ACT_LOG_DIR } else { Join-Path $RepoRoot ".act/logs" }
  New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
  $Jobs = @()
  foreach ($Name in @("cmake", "hygiene", "lint", "linux-pack", "macos-pack", "windows-pack")) {
    $LogFile = Join-Path $LogDir "$Name.log"
    Write-Host "starting $Name -> $LogFile"
    $Jobs += Start-Job -ScriptBlock {
      param($ScriptPath, $WorkflowName, $Arguments, $OutFile)
      & pwsh $ScriptPath $WorkflowName @Arguments *> $OutFile
      if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
    } -ArgumentList $PSCommandPath, $Name, $ExtraArgs, $LogFile
  }

  $Failed = $false
  foreach ($Job in $Jobs) {
    Wait-Job $Job | Out-Null
    $Code = Receive-Job $Job
    if ($Job.State -ne "Completed" -or $Code -ne 0) {
      $Failed = $true
    }
    Remove-Job $Job
  }
  if ($Failed) {
    exit 1
  }
}

function Invoke-DryRun([string[]]$Args) {
  if ($Args.Count -lt 1) {
    Show-Usage
    throw "dry-run needs a workflow command"
  }
  $Target = $Args[0]
  $Rest = @($Args | Select-Object -Skip 1)
  if ($Target -eq "all-series") {
    Invoke-Series (@("--dryrun") + $Rest)
    return
  }
  Invoke-Named $Target (@("--dryrun") + $Rest)
}

function Show-WorkflowList {
  Require-Act
  foreach ($Name in @("cmake", "hygiene", "lint", "linux-pack", "macos-pack")) {
    & $PSCommandPath $Name --list
  }
  & $PSCommandPath windows-pack --list
}

if (-not $Command) {
  Show-Usage
  exit 2
}

switch ($Command) {
  { $_ -in @("cmake", "hygiene", "lint", "linux-pack", "macos-pack", "windows-pack") } {
    Invoke-Named $Command $ActArgs
  }
  "all-series" { Invoke-Series $ActArgs }
  "all-parallel" { Invoke-Parallel $ActArgs }
  "dry-run" { Invoke-DryRun $ActArgs }
  "list" { Show-WorkflowList }
  { $_ -in @("-h", "--help", "help") } { Show-Usage }
  default {
    Show-Usage
    throw "unknown command: $Command"
  }
}
