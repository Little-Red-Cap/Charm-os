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

    Write-Output "[armv7a-qemu-arch-ingress-seam] log tail:"
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
$outFile = Join-Path $PSScriptRoot "qemu-arch-ingress-seam.log"
$errFile = Join-Path $PSScriptRoot "qemu-arch-ingress-seam.err.log"

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
$missing = [System.Collections.Generic.List[string]]::new()

$expectedPhases = @(
    "ARMv7-A phase, stage=svc-smoke",
    "ARMv7-A phase, stage=timer-irq-smoke",
    "ARMv7-A phase, stage=sgi-irq-smoke",
    "ARMv7-A phase, stage=sgi-fiq-smoke",
    "ARMv7-A phase, stage=kernel-ingress",
    "ARMv7-A phase, stage=scheduler-tick-ingress",
    "ARMv7-A phase, stage=runtime-trap-ingress",
    "ARMv7-A phase, stage=context-switch-smoke",
    "ARMv7-A phase, stage=runtime-loop-ingress"
)
foreach ($phase in @($expectedPhases)) {
    if (-not $log.Contains($phase)) {
        $missing.Add($phase) | Out-Null
    }
}

$requiredPatterns = @(
    "ARMv7-A return evidence, vector=svc, origin-mode=sys, current-mode=sys, origin-irq=masked, current-irq=masked, origin-fiq=masked, current-fiq=masked, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x[0-9A-F]{8}, base=0x[0-9A-F]{8}, top=0x[0-9A-F]{8}, used=0x[0-9A-F]{8}, in-range=yes",
    "ARMv7-A timer IRQ active, intid=30, source=non-secure-phys-ppi, ack=0x[0-9A-F]{8}, hppir-before-ack=0x[0-9A-F]{8}, line=group1/yes/yes/yes, origin-mode=sys, handler-mode=irq, return-pc=0x[0-9A-F]{8}",
    "ARMv7-A timer IRQ lifecycle, intid=30, source=non-secure-phys-ppi, entry-match=yes, retired=yes, restored=yes, closed=yes",
    "ARMv7-A SGI active, intid=1, source=self-sgi, ack=0x[0-9A-F]{8}, hppir-before-ack=0x[0-9A-F]{8}, line=group1/yes/no/yes, origin-mode=sys, handler-mode=irq, return-pc=0x[0-9A-F]{8}",
    "ARMv7-A security side evidence, scr-read=skipped, timer-source=non-secure-phys-ppi/group1, irq-source=self-sgi/group1, irq-origin=sys, irq-handler=irq, fiq-source=self-sgi/group0, fiq-origin=sys, fiq-handler=fiq, monitor-mode=not-observed",
    "ARMv7-A kernel ingress, vector-base=0x[0-9A-F]{8}, tick-mode=oneshot, tick-route=irq, timer-hz=62500000, exception=yes, interrupt=yes, timer=yes, context-ready=yes, context-model=software-frame, tick-runtime=yes, thread-runtime=yes",
    "ARMv7-A scheduler tick ingress, source=timer-irq, route=irq, mode=oneshot, intid=30, hz=62500000, now=0x[0-9A-F]{16}, source-match=yes, counter=yes, isr-safe=yes, retired=yes, handoff=yes, rearm=yes",
    "ARMv7-A runtime trap context, yield-path=context-port, yield-task=0x[0-9A-F]{16}, yield-sp=0x[0-9A-F]{16}, yield-ready=yes, sleep-path=context-port, sleep-task=0x[0-9A-F]{16}, sleep-sp=0x[0-9A-F]{16}, sleep-ready=yes, context=yes",
    "ARMv7-A runtime trap ingress, source=svc, service=0x000043, arg0=0x00000001, arg1=0x00000001, arg2=0x00000000, arg3=0x00000000, service-ready=yes, args-ready=yes, trap=yes",
    "ARMv7-A runtime trap dispatch, yield-path=dispatch-port, yield-generic=0x0001, yield-r0=0x00000001, yield-task=0x0000000056430001, yield-sp=0x[0-9A-F]{16}, yield-ready=yes, sleep-path=dispatch-port, sleep-generic=0x0002, sleep-r0=0x00000005, sleep-task=0x0000000056430001, sleep-sp=0x[0-9A-F]{16}, sleep-ready=yes, dispatch=yes",
    "ARMv7-A runtime trap roundtrip, yield-path=svc-return, yield-svc=0x000043, yield-value=0x00000001, yield-ready=yes, sleep-path=svc-return, sleep-svc=0x000044, sleep-value=0x00000005, sleep-ready=yes, roundtrip=yes",
    "ARMv7-A runtime trap failure, unsupported=unsupported-service, decode=decode-failed, writeback=writeback-failed, adapter=unbound-adapter, dispatch=unbound-adapter, failure=yes",
    "ARMv7-A context switch smoke, main-before=0x[0-9A-F]{8}, main-saved=0x[0-9A-F]{8}, thread-entry-sp=0x[0-9A-F]{8}, thread-saved=0x[0-9A-F]{8}, thread-resume-sp=0x[0-9A-F]{8}, entry=yes, resumed=yes, round-trip=yes",
    "ARMv7-A runtime bridge, tick=yes, isr-defer=yes, yield-svc=0x000043, yield-event=0x00000001, yield-payload=0x00000001, yield-ready=yes, sleep-svc=0x000044, sleep-due=0x0000000000000005, sleep-event=0x00000001, sleep-payload=0x00000005, sleep-ready=yes, dispatch=yes, bridge=yes",
    "ARMv7-A runtime loop ingress, mode=oneshot, route=irq, hz=62500000, tick-runtime=yes, thread=yes, tick=yes, isr-defer=yes, idle=yes, worker=yes, run=yes, loop=yes"
)
foreach ($pattern in @($requiredPatterns)) {
    if ($log -notmatch $pattern) {
        $missing.Add($pattern) | Out-Null
    }
}

if ($missing.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines
    throw ("missing expected output: {0}" -f ($missing -join '; '))
}

Write-Output "[ok] armv7a qemu arch-ingress seam smoke detected"
