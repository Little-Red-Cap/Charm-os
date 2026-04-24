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

    Write-Output "[armv7a-qemu-handoff] log tail:"
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
$outFile = Join-Path $PSScriptRoot "qemu-handoff.log"
$errFile = Join-Path $PSScriptRoot "qemu-handoff.err.log"

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
    "ARMv7-A phase, stage=handoff-prepare",
    "ARMv7-A phase complete, stage=handoff-prepare",
    "ARMv7-A phase, stage=handoff-transfer",
    "ARMv7-A phase complete, stage=handoff-transfer",
    "ARMv7-A phase, stage=handoff-launch",
    "ARMv7-A phase complete, stage=handoff-launch",
    "ARMv7-A phase, stage=idle"
)
$missing = $expected | Where-Object { -not $log.Contains($_) }

$phaseMarker = "ARMv7-A phase, stage=handoff-prepare"
$nextPhaseMarker = "ARMv7-A phase, stage=idle"
$phaseIndex = $log.IndexOf($phaseMarker)
$nextPhaseIndex = if ($phaseIndex -ge 0) {
    $log.IndexOf($nextPhaseMarker, $phaseIndex)
} else {
    -1
}
$handoffLog = if ($phaseIndex -ge 0) {
    if ($nextPhaseIndex -gt $phaseIndex) {
        $log.Substring($phaseIndex, $nextPhaseIndex - $phaseIndex)
    } else {
        $log.Substring($phaseIndex)
    }
} else {
    $log
}

if (($handoffLog -notmatch "ARMv7-A runtime handoff, runtime=yes, context=yes, hooks=yes, vector=yes, report=yes, export=yes, handoff=yes")) {
    $missing += "ARMv7-A runtime handoff, runtime=yes..."
}
if (($handoffLog -notmatch "ARMv7-A handoff entry, target=0x[0-9A-F]{8}, request=yes, offset=yes, mode=sys, vector=yes, translation=yes, cache=yes, masks=yes, export=yes, entry=yes")) {
    $missing += "ARMv7-A handoff entry, target=0x..."
}
if (($handoffLog -notmatch "ARMv7-A handoff transfer, target=0x[0-9A-F]{8}, arg0=0x[0-9A-F]{8}, size=0x[0-9A-F]{8}, mode=sys, state=arm, entry=yes, payload=yes, stack=yes, export=yes, transfer=yes")) {
    $missing += "ARMv7-A handoff transfer, target=0x..."
}
if (($handoffLog -notmatch "ARMv7-A handoff launch, target=0x[0-9A-F]{8}, arg0=0x[0-9A-F]{8}, mode=sys, state=arm, transfer=yes, hook=yes, route=yes, probe=yes, arg0=yes, next=yes, stack=yes, branch=yes, link=yes, return=yes, invoke=yes, launch=yes")) {
    $missing += "ARMv7-A handoff launch, target=0x..."
}
if (($handoffLog -notmatch "ARMv7-A handoff context, vector-base=0x[0-9A-F]{8}, translation-table=0x[0-9A-F]{8}, image-base=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A handoff context, vector-base=0x..."
}
if (($handoffLog -notmatch "ARMv7-A handoff request, kind=(copy|xip), payload-base=0x[0-9A-F]{8}, entry=0x[0-9A-F]{8}, storage-payload=0x[0-9A-F]{8}, storage-entry=0x[0-9A-F]{8}, entry-offset=0x[0-9A-F]{8}, payload-size=0x[0-9A-F]{8}, image-size=0x[0-9A-F]{8}, flags=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A handoff request, kind=..."
}
if (($handoffLog -notmatch "ARMv7-A handoff steps, mask=yes, quiesce=yes, map=yes, dcache=yes, icache=yes, tlb=yes, vectors=yes, sync=yes")) {
    $missing += "ARMv7-A handoff steps, mask=yes..."
}
if (($handoffLog -notmatch "ARMv7-A handoff ready, result=yes, vbar=0x[0-9A-F]{8}, ttbr0=0x[0-9A-F]{8}, ttbcr=0x[0-9A-F]{8}, dacr=0x[0-9A-F]{8}, mmu=on, dcache=on, icache=on, irq=masked, fiq=masked")) {
    $missing += "ARMv7-A handoff ready, result=yes..."
}

if ($missing.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines
    throw "missing expected output: $($missing -join '; ')"
}

Write-Output "[ok] armv7a qemu handoff smoke detected"
