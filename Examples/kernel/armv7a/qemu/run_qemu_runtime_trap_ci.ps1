param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "qemu-system-arm",
    [int]$BuildJobs = 1,
    [int]$TimeoutSec = 10,
    [int]$TailLines = 40,
    [switch]$SkipBuild
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

    Write-Output "[armv7a-qemu-runtime-trap] log tail:"
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

if (-not $SkipBuild) {
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
}

$elf = Resolve-ExamplePath -Path $elfPath
$outFile = Join-Path $PSScriptRoot "qemu-runtime-trap.log"
$errFile = Join-Path $PSScriptRoot "qemu-runtime-trap.err.log"

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
    -RedirectStandardError $errFile -WindowStyle Hidden -PassThru

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
}

$log = [string](Read-LogSafe -Path $outFile) + [string](Read-LogSafe -Path $errFile)
$expected = @(
    "Charm ARMv7-A QEMU skeleton",
    "Targeting Cortex-A7 first, RK3506 later.",
    "ARMv7-A phase, stage=runtime-trap-frame",
    "ARMv7-A phase complete, stage=runtime-trap-frame",
    "ARMv7-A phase, stage=runtime-trap-ingress",
    "ARMv7-A phase complete, stage=runtime-trap-ingress",
    "ARMv7-A phase, stage=runtime-trap-mapping",
    "ARMv7-A phase complete, stage=runtime-trap-mapping",
    "ARMv7-A phase, stage=runtime-trap-adapter",
    "ARMv7-A phase complete, stage=runtime-trap-adapter",
    "ARMv7-A phase, stage=runtime-trap-seam",
    "ARMv7-A phase complete, stage=runtime-trap-seam",
    "ARMv7-A phase, stage=runtime-trap-live-adapter",
    "ARMv7-A phase complete, stage=runtime-trap-live-adapter",
    "ARMv7-A phase, stage=runtime-trap-ingress-adapter",
    "ARMv7-A phase complete, stage=runtime-trap-ingress-adapter",
    "ARMv7-A phase, stage=runtime-trap-caller",
    "ARMv7-A phase complete, stage=runtime-trap-caller",
    "ARMv7-A phase, stage=runtime-trap-dispatch",
    "ARMv7-A phase complete, stage=runtime-trap-dispatch",
    "ARMv7-A phase, stage=runtime-current",
    "ARMv7-A phase complete, stage=runtime-current",
    "ARMv7-A phase, stage=runtime-trap-context",
    "ARMv7-A phase complete, stage=runtime-trap-context",
    "ARMv7-A phase, stage=runtime-trap-roundtrip",
    "ARMv7-A phase complete, stage=runtime-trap-roundtrip",
    "ARMv7-A phase, stage=runtime-trap-failure",
    "ARMv7-A phase complete, stage=runtime-trap-failure",
    "ARMv7-A phase, stage=context-switch-smoke"
)
$missing = $expected | Where-Object { -not $log.Contains($_) }

$phaseMarker = "ARMv7-A phase, stage=runtime-trap-frame"
$nextPhaseMarker = "ARMv7-A phase, stage=context-switch-smoke"
$phaseIndex = $log.IndexOf($phaseMarker)
$nextPhaseIndex = if ($phaseIndex -ge 0) {
    $log.IndexOf($nextPhaseMarker, $phaseIndex)
} else {
    -1
}
$runtimeTrapLog = if ($phaseIndex -ge 0) {
    if ($nextPhaseIndex -gt $phaseIndex) {
        $log.Substring($phaseIndex, $nextPhaseIndex - $phaseIndex)
    } else {
        $log.Substring($phaseIndex)
    }
} else {
    $log
}

if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap frame, yield-path=svc-frame, yield-handler=svc, yield-return-pc=0x[0-9A-F]{8}, yield-ready=yes, sleep-path=svc-frame, sleep-handler=svc, sleep-return-pc=0x[0-9A-F]{8}, sleep-ready=yes, frame=yes")) {
    $missing += "ARMv7-A runtime trap frame, yield-path=svc-frame..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap ingress, source=svc, service=0x000043, arg0=0x00000001, arg1=0x00000001, arg2=0x00000000, arg3=0x00000000, service-ready=yes, args-ready=yes, trap=yes")) {
    $missing += "ARMv7-A runtime trap ingress, source=svc..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap mapping, yield=yield-current, yield-generic=0x0001, yield-origin=kernel-thread, yield-return-pc=0x[0-9A-F]{8}, yield-ready=yes, sleep=sleep-until, sleep-generic=0x0002, sleep-origin=kernel-thread, sleep-due=0x0000000000000005, sleep-ready=yes, mapping=yes")) {
    $missing += "ARMv7-A runtime trap mapping, yield=yield-current..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap adapter, yield-path=svc-r0, yield-r0=0x00000001, yield-preserve=yes, yield-ready=yes, sleep-path=svc-r0, sleep-r0=0x00000005, sleep-preserve=yes, sleep-ready=yes, adapter=yes")) {
    $missing += "ARMv7-A runtime trap adapter, yield-path=svc-r0..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap seam, yield-path=svc-frame-r0, yield-generic=0x0001, yield-origin=kernel-thread, yield-r0=0x00000001, yield-ready=yes, sleep-path=svc-frame-r0, sleep-generic=0x0002, sleep-origin=kernel-thread, sleep-r0=0x00000005, sleep-ready=yes, seam=yes")) {
    $missing += "ARMv7-A runtime trap seam, yield-path=svc-frame-r0..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap live-adapter, yield-path=svc-live-frame, yield-generic=0x0001, yield-r0=0x00000001, yield-ready=yes, sleep-path=svc-live-frame, sleep-generic=0x0002, sleep-r0=0x00000005, sleep-ready=yes, live-adapter=yes")) {
    $missing += "ARMv7-A runtime trap live-adapter, yield-path=svc-live-frame..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap ingress-adapter, yield-path=live-frame-adapter, yield-generic=0x0001, yield-r0=0x00000001, yield-ready=yes, sleep-path=live-frame-adapter, sleep-generic=0x0002, sleep-r0=0x00000005, sleep-ready=yes, ingress-adapter=yes")) {
    $missing += "ARMv7-A runtime trap ingress-adapter, yield-path=live-frame-adapter..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap caller, yield-path=svc-call-frame, yield-svc=0x000043, yield-r0=0x00000001, yield-ready=yes, sleep-path=svc-call-frame, sleep-svc=0x000044, sleep-due=0x0000000000000005, sleep-r0=0x00000005, sleep-ready=yes, caller=yes")) {
    $missing += "ARMv7-A runtime trap caller, yield-path=svc-call-frame..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap dispatch, yield-path=dispatch-port, yield-generic=0x0001, yield-r0=0x00000001, yield-task=0x0000000056430001, yield-sp=0x[0-9A-F]{16}, yield-ready=yes, sleep-path=dispatch-port, sleep-generic=0x0002, sleep-r0=0x00000005, sleep-task=0x0000000056430001, sleep-sp=0x[0-9A-F]{16}, sleep-ready=yes, dispatch=yes")) {
    $missing += "ARMv7-A runtime trap dispatch, yield-path=dispatch-port..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime current, path=current-slot, task=0x0000000013572468, sp=0x0000000052001000, task-valid=yes, current=yes")) {
    $missing += "ARMv7-A runtime current, path=current-slot..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap context, yield-path=context-port, yield-task=0x0000000013572468, yield-sp=0x0000000052001000, yield-ready=yes, sleep-path=context-port, sleep-task=0x0000000013572468, sleep-sp=0x0000000052001000, sleep-ready=yes, context=yes")) {
    $missing += "ARMv7-A runtime trap context, yield-path=context-port..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap roundtrip, yield-path=svc-return, yield-svc=0x000043, yield-value=0x00000001, yield-ready=yes, sleep-path=svc-return, sleep-svc=0x000044, sleep-value=0x00000005, sleep-ready=yes, roundtrip=yes")) {
    $missing += "ARMv7-A runtime trap roundtrip, yield-path=svc-return..."
}
if (($runtimeTrapLog -notmatch "ARMv7-A runtime trap failure, unsupported=unsupported-service, decode=decode-failed, writeback=writeback-failed, adapter=unbound-adapter, dispatch=unbound-adapter, failure=yes")) {
    $missing += "ARMv7-A runtime trap failure, unsupported=unsupported-service..."
}

if ($missing.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines
    throw "missing expected output: $($missing -join '; ')"
}

Write-Output "[ok] armv7a qemu runtime-trap smoke detected"
