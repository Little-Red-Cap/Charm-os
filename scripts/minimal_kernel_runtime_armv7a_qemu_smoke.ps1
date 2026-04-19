param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "qemu-system-arm",
    [int]$BuildJobs = 1,
    [int]$TimeoutSec = 10,
    [int]$TailLines = 40
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
$leafScript = Join-Path $repoRoot "Examples/kernel/armv7a/qemu/run_qemu_lower_half_ci.ps1"

if (-not (Test-Path $leafScript)) {
    throw "missing lower-half smoke script: $leafScript"
}

Push-Location $repoRoot
try {
    & $leafScript `
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

Write-Output "[ok] minimal kernel ARMv7-A QEMU smoke detected"
