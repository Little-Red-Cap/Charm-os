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

function Resolve-ExamplePath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path $Path).Path
    }

    return (Resolve-Path (Join-Path $PSScriptRoot $Path)).Path
}

function Read-LogSafe {
    param([string]$Path)

    if (-not (Test-Path $Path)) {
        return ""
    }

    try {
        return [string](Get-Content $Path -Raw)
    } catch {
        return ""
    }
}

function Show-LogTail {
    param(
        [string]$OutPath,
        [string]$ErrPath,
        [int]$Lines
    )

    Write-Output "[armv7a-qemu-task-syscall] log tail:"
    if (Test-Path $OutPath) {
        Get-Content $OutPath -Tail $Lines
    }
    if (Test-Path $ErrPath) {
        Get-Content $ErrPath -Tail $Lines
    }
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$configurePreset = "debug"
$buildPreset = "debug"
$elfPath = "out\\build\\debug\\charm-armv7a-qemu"

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

$elf = Resolve-ExamplePath -Path $elfPath
$outFile = Join-Path $PSScriptRoot "qemu-task-syscall.log"
$errFile = Join-Path $PSScriptRoot "qemu-task-syscall.err.log"

Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue

$args = @(
    "-machine", "virt",
    "-cpu", "cortex-a7",
    "-display", "none",
    "-serial", "file:$outFile",
    "-monitor", "none",
    "-device", "loader,file=$elf,cpu-num=0"
)

$proc = Start-Process -FilePath $qemu -ArgumentList $args `
    -RedirectStandardError $errFile -PassThru

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
}

$log = [string](Read-LogSafe -Path $outFile) + [string](Read-LogSafe -Path $errFile)
$expected = @(
    "Charm ARMv7-A QEMU skeleton",
    "Targeting Cortex-A7 first, RK3506 later.",
    "ARMv7-A phase, stage=task-syscall-frame",
    "ARMv7-A phase complete, stage=task-syscall-frame",
    "ARMv7-A phase, stage=task-syscall-dispatch",
    "ARMv7-A phase complete, stage=task-syscall-dispatch",
    "ARMv7-A phase, stage=task-syscall-surface",
    "ARMv7-A phase complete, stage=task-syscall-surface",
    "ARMv7-A phase, stage=task-syscall-ingress-adapter",
    "ARMv7-A phase complete, stage=task-syscall-ingress-adapter",
    "ARMv7-A phase, stage=task-syscall-caller",
    "ARMv7-A phase complete, stage=task-syscall-caller",
    "ARMv7-A phase, stage=task-syscall-roundtrip",
    "ARMv7-A phase complete, stage=task-syscall-roundtrip",
    "ARMv7-A phase, stage=task-syscall-glue",
    "ARMv7-A phase complete, stage=task-syscall-glue",
    "ARMv7-A phase, stage=task-syscall-failure",
    "ARMv7-A phase complete, stage=task-syscall-failure",
    "ARMv7-A phase, stage=handoff-prepare"
)
$missing = $expected | Where-Object { -not $log.Contains($_) }

$phaseMarker = "ARMv7-A phase, stage=task-syscall-frame"
$nextPhaseMarker = "ARMv7-A phase, stage=handoff-prepare"
$phaseIndex = $log.IndexOf($phaseMarker)
$nextPhaseIndex = if ($phaseIndex -ge 0) {
    $log.IndexOf($nextPhaseMarker, $phaseIndex)
} else {
    -1
}
$taskSyscallLog = if ($phaseIndex -ge 0) {
    if ($nextPhaseIndex -gt $phaseIndex) {
        $log.Substring($phaseIndex, $nextPhaseIndex - $phaseIndex)
    } else {
        $log.Substring($phaseIndex)
    }
} else {
    $log
}

if (($taskSyscallLog -notmatch "ARMv7-A task syscall frame, debug-path=svc-frame, debug-svc=0x000045, debug-generic=0x0003, debug-task=0x0000000059532001, debug-ready=yes, capability-path=svc-frame, capability-svc=0x000046, capability-generic=0x0004, capability-task=0x0000000059532001, capability-ready=yes, frame=yes")) {
    $missing += "ARMv7-A task syscall frame, debug-path=svc-frame..."
}
if (($taskSyscallLog -notmatch "ARMv7-A task syscall dispatch, debug-path=dispatch-port, debug-generic=0x0003, debug-task=0x0000000059533001, debug-r0=0x00000044, debug-ready=yes, capability-path=dispatch-port, capability-generic=0x0004, capability-task=0x0000000059533001, capability-r0=0x0000002A, capability-ready=yes, dispatch=yes")) {
    $missing += "ARMv7-A task syscall dispatch, debug-path=dispatch-port..."
}
if (($taskSyscallLog -notmatch "ARMv7-A task syscall surface, debug-path=live-svc-dispatch, debug-svc=0x000045, debug-generic=0x0003, debug-r0=0x00000044, debug-ready=yes, capability-path=live-svc-dispatch, capability-svc=0x000046, capability-generic=0x0004, capability-r0=0x0000002A, capability-ready=yes, surface=yes")) {
    $missing += "ARMv7-A task syscall surface, debug-path=live-svc-dispatch..."
}
if (($taskSyscallLog -notmatch "ARMv7-A task syscall ingress-adapter, debug-path=live-frame-adapter, debug-generic=0x0003, debug-r0=0x00000044, debug-ready=yes, capability-path=live-frame-adapter, capability-generic=0x0004, capability-r0=0x0000002A, capability-ready=yes, ingress-adapter=yes")) {
    $missing += "ARMv7-A task syscall ingress-adapter, debug-path=live-frame-adapter..."
}
if (($taskSyscallLog -notmatch "ARMv7-A task syscall caller, debug-path=svc-call-frame, debug-svc=0x000045, debug-generic=0x0003, debug-r0=0x00000044, debug-ready=yes, capability-path=svc-call-frame, capability-svc=0x000046, capability-generic=0x0004, capability-r0=0x0000002A, capability-ready=yes, caller=yes")) {
    $missing += "ARMv7-A task syscall caller, debug-path=svc-call-frame..."
}
if (($taskSyscallLog -notmatch "ARMv7-A task syscall roundtrip, debug-path=svc-return, debug-svc=0x000045, debug-value=0x00000044, debug-ready=yes, capability-path=svc-return, capability-svc=0x000046, capability-value=0x0000002A, capability-ready=yes, roundtrip=yes")) {
    $missing += "ARMv7-A task syscall roundtrip, debug-path=svc-return..."
}
if (($taskSyscallLog -notmatch "ARMv7-A task syscall glue, task=0x0000000059534001, stack=0x0000000052008000, yield=0x00000001, sleep=0x00000037, debug=0x000000CD, capability=0x0000002A, generic=yes, ingress=yes, bridge=yes, caller=yes, api=yes, glue=yes")) {
    $missing += "ARMv7-A task syscall glue, task=0x0000000059534001..."
}
if (($taskSyscallLog -notmatch "ARMv7-A task syscall failure, decode=decode-failed, unsupported=unsupported-service, bridge=unbound-bridge, caller=unbound-adapter, writeback=writeback-failed, failure=yes")) {
    $missing += "ARMv7-A task syscall failure, decode=decode-failed..."
}

if ($missing.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines
    throw "missing expected output: $($missing -join '; ')"
}

Write-Output "[ok] armv7a qemu task-syscall smoke detected"
