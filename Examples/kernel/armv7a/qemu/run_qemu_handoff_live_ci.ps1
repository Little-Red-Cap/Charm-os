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

    Write-Output "[armv7a-qemu-handoff-live] log tail:"
    if (Test-Path $OutPath) {
        Get-Content $OutPath -Tail $Lines
    }
    if (Test-Path $ErrPath) {
        Get-Content $ErrPath -Tail $Lines
    }
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$configurePreset = "debug-handoff-live"
$buildPreset = "debug-handoff-live"
$elfPath = "out\\build\\debug-handoff-live\\charm-armv7a-qemu"

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
$outFile = Join-Path $PSScriptRoot "qemu-handoff-live.log"
$errFile = Join-Path $PSScriptRoot "qemu-handoff-live.err.log"

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
    "ARMv7-A phase, stage=handoff-live",
    "ARMv7-A phase complete, stage=handoff-live",
    "ARMv7-A phase, stage=handoff-runtime",
    "ARMv7-A phase complete, stage=handoff-runtime",
    "ARMv7-A phase, stage=idle"
)
$missing = $expected | Where-Object { -not $log.Contains($_) }

$phaseMarker = "ARMv7-A phase, stage=handoff-launch"
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

if (($handoffLog -notmatch "ARMv7-A handoff live, target=0x[0-9A-F]{8}, arg0=0x[0-9A-F]{8}, mode=sys, state=arm, transfer=yes, route=yes, target=yes, arg0=yes, stack=yes, mode=yes, state=yes, live=yes")) {
    $missing += "ARMv7-A handoff live, target=0x..."
}
if (($handoffLog -notmatch "ARMv7-A runtime handoff landing, package=yes, rearm=yes, payload=yes, binding=yes, current=yes, trap=yes, thread=yes, loop=yes, live=yes, landed=yes")) {
    $missing += "ARMv7-A runtime handoff landing, package=yes..."
}

$unexpected = @()
if ($handoffLog.Contains("ARMv7-A handoff launch, ")) {
    $unexpected += "ARMv7-A handoff launch, ..."
}

if ($missing.Count -gt 0 -or $unexpected.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines

    $details = @()
    if ($missing.Count -gt 0) {
        $details += "missing expected output: $($missing -join '; ')"
    }
    if ($unexpected.Count -gt 0) {
        $details += "unexpected output during handoff-live smoke: $($unexpected -join '; ')"
    }

    throw ($details -join ' | ')
}

Write-Output "[ok] armv7a qemu handoff-live smoke detected"
