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

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$configurePreset = "debug"
$buildPreset = "debug"

Push-Location $PSScriptRoot
try {
    & $cmake --preset $configurePreset
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed for preset: $configurePreset"
    }

    & $cmake --build --preset $buildPreset --parallel $BuildJobs
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed for preset: $buildPreset"
    }
} finally {
    Pop-Location
}

$scripts = @(
    "run_qemu_runtime_trap_ci.ps1",
    "run_qemu_runtime_leaf_ports_ci.ps1",
    "run_qemu_runtime_live_ci.ps1",
    "run_qemu_runtime_leaf_bundle_ci.ps1",
    "run_qemu_task_syscall_ci.ps1"
)

foreach ($scriptName in $scripts) {
    $scriptPath = Join-Path $PSScriptRoot $scriptName
    & $scriptPath `
        -CMakeExe $cmake `
        -QemuExe $qemu `
        -BuildJobs $BuildJobs `
        -TimeoutSec $TimeoutSec `
        -TailLines $TailLines `
        -SkipBuild
    if ($LASTEXITCODE -ne 0) {
        throw "lower-half smoke failed: $scriptName"
    }
}

Write-Output "[ok] armv7a qemu lower-half smoke detected"
