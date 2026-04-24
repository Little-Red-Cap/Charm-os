param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "qemu-system-arm",
    [string]$Generator = "Ninja",
    [int]$BuildJobs = 1,
    [int]$TimeoutSec = 10,
    [int]$TailLines = 40,
    [switch]$KeepBuildDirs
)

$ErrorActionPreference = "Stop"

function Resolve-ToolPath {
    param([string]$Tool)

    $cmd = Get-Command $Tool -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    if (Test-Path $Tool) {
        return (Resolve-Path $Tool).Path
    }

    throw "tool not found: $Tool"
}

function Resolve-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$repoRoot = Resolve-RepoRoot
$hostSmoke = Join-Path $repoRoot "scripts/minimal_kernel_runtime_host_smoke.ps1"
$armv7aSmoke = Join-Path $repoRoot "scripts/minimal_kernel_runtime_armv7a_qemu_smoke.ps1"

foreach ($script in @($hostSmoke, $armv7aSmoke)) {
    if (-not (Test-Path $script)) {
        throw "required smoke script not found: $script"
    }
}

Push-Location $repoRoot
try {
    & $hostSmoke -CMakeExe $cmake -Generator $Generator -KeepBuildDirs:$KeepBuildDirs
    if ($LASTEXITCODE -ne 0) {
        throw "minimal kernel host smoke failed"
    }

    & $armv7aSmoke `
        -CMakeExe $cmake `
        -QemuExe $qemu `
        -BuildJobs $BuildJobs `
        -TimeoutSec $TimeoutSec `
        -TailLines $TailLines
    if ($LASTEXITCODE -ne 0) {
        throw "ARMv7-A lower-half smoke failed"
    }
} finally {
    Pop-Location
}

Write-Output "[ok] minimal kernel runtime smoke detected"
