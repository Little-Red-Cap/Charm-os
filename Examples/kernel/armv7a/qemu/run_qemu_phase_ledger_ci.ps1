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

    Write-Output "[armv7a-qemu-phase-ledger] log tail:"
    if (Test-Path $OutPath) {
        Get-Content $OutPath -Tail $Lines
    }
    if (Test-Path $ErrPath) {
        Get-Content $ErrPath -Tail $Lines
    }
}

function Assert-OrderedMarker {
    param(
        [string]$Log,
        [string]$Marker,
        [int]$AfterIndex,
        [System.Collections.Generic.List[string]]$Missing
    )

    $index = $Log.IndexOf($Marker, $AfterIndex + 1)
    if ($index -lt 0) {
        $Missing.Add($Marker)
        return $AfterIndex
    }

    return $index
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$configurePreset = "debug"
$buildPreset = "debug"
$elfPath = "out\build\debug\charm-armv7a-qemu"

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
$outFile = Join-Path $PSScriptRoot "qemu-phase-ledger.log"
$errFile = Join-Path $PSScriptRoot "qemu-phase-ledger.err.log"

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

foreach ($banner in @(
    "Charm ARMv7-A QEMU skeleton",
    "Targeting Cortex-A7 first, RK3506 later."
)) {
    if (-not $log.Contains($banner)) {
        $missing.Add($banner)
    }
}

$orderedPhases = @(
    "boot-cpu-state",
    "memory-probe-prepare",
    "memory-probe-describe",
    "mmu-activate",
    "small-page-probe",
    "attribute-probe",
    "icache-probe",
    "abort-smoke",
    "exception-smoke",
    "dcache-probe",
    "page-table-probe",
    "section-split-probe",
    "svc-smoke",
    "timer-irq-smoke",
    "sgi-irq-smoke",
    "sgi-fiq-smoke",
    "kernel-ingress",
    "scheduler-tick-ingress",
    "runtime-trap-frame",
    "runtime-trap-ingress",
    "runtime-trap-mapping",
    "runtime-trap-adapter",
    "runtime-trap-seam",
    "runtime-trap-live-adapter",
    "runtime-trap-ingress-adapter",
    "runtime-trap-caller",
    "runtime-trap-dispatch",
    "runtime-current",
    "runtime-trap-context",
    "runtime-trap-roundtrip",
    "runtime-trap-failure",
    "context-switch-smoke",
    "thread-runtime",
    "scheduler-dispatch",
    "runtime-bridge",
    "runtime-loop-ingress",
    "runtime-leaf-ports",
    "runtime-thread-port",
    "runtime-live",
    "runtime-binding-bundle",
    "runtime-leaf-bundle",
    "runtime-package",
    "task-syscall-frame",
    "task-syscall-dispatch",
    "task-syscall-surface",
    "task-syscall-ingress-adapter",
    "task-syscall-caller",
    "task-syscall-roundtrip",
    "task-syscall-glue",
    "task-syscall-failure",
    "handoff-prepare",
    "handoff-transfer",
    "handoff-launch",
    "idle"
)

$cursor = -1
foreach ($phase in $orderedPhases) {
    $enterMarker = "ARMv7-A phase, stage=$phase"
    $cursor = Assert-OrderedMarker -Log $log -Marker $enterMarker -AfterIndex $cursor -Missing $missing

    if ($phase -ne "idle") {
        $completeMarker = "ARMv7-A phase complete, stage=$phase"
        $cursor = Assert-OrderedMarker -Log $log -Marker $completeMarker -AfterIndex $cursor -Missing $missing
    }
}

$unexpectedPhases = @(
    "special-irq-smoke",
    "sgi-irq-timeout-smoke",
    "unexpected-irq-smoke",
    "sgi-fiq-timeout-smoke",
    "handoff-live",
    "handoff-runtime"
)
foreach ($phase in $unexpectedPhases) {
    if ($log.Contains("ARMv7-A phase, stage=$phase")) {
        $missing.Add("unexpected default phase: $phase")
    }
}

$phaseLineCount = ([regex]::Matches($log, "ARMv7-A phase, stage=")).Count
$completeLineCount = ([regex]::Matches($log, "ARMv7-A phase complete, stage=")).Count
$expectedEnterCount = $orderedPhases.Count
$expectedCompleteCount = $orderedPhases.Count - 1
if ($phaseLineCount -ne $expectedEnterCount) {
    $missing.Add("phase enter count: expected $expectedEnterCount but got $phaseLineCount")
}
if ($completeLineCount -ne $expectedCompleteCount) {
    $missing.Add("phase complete count: expected $expectedCompleteCount but got $completeLineCount")
}

foreach ($marker in @(
    "ARMv7-A runtime live, task=yes, trap=yes, timer=yes, tick=yes, idle=yes, worker=yes, live=yes",
    "ARMv7-A runtime package, leaf=yes, binding=yes, current=yes, trap=yes, call=yes, thread=yes, loop=yes, live=yes, derived=yes, export=yes, package=yes",
    "ARMv7-A task syscall glue, task=0x0000000059534001",
    "ARMv7-A handoff launch, target=0x"
)) {
    if (-not $log.Contains($marker)) {
        $missing.Add($marker)
    }
}

if ($missing.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines
    throw "phase ledger mismatch: $($missing -join '; ')"
}

Write-Output "[ok] armv7a qemu phase-ledger smoke detected phases=$expectedEnterCount completed=$expectedCompleteCount"
