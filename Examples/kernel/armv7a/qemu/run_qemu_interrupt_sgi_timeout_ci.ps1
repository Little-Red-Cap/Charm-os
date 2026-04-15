param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "qemu-system-arm",
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
        return Get-Content $Path -Raw
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

    Write-Output "[armv7a-qemu-interrupt-timeout] log tail:"
    if (Test-Path $OutPath) {
        Get-Content $OutPath -Tail $Lines
    }
    if (Test-Path $ErrPath) {
        Get-Content $ErrPath -Tail $Lines
    }
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$configurePreset = "debug-interrupt-sgi-timeout"
$buildPreset = "debug-interrupt-sgi-timeout"
$elfPath = "out\\build\\debug-interrupt-sgi-timeout\\charm-armv7a-qemu"

Push-Location $PSScriptRoot
try {
    & $cmake --preset $configurePreset
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed for preset: $configurePreset"
    }

    & $cmake --build --preset $buildPreset --verbose
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed for preset: $buildPreset"
    }
} finally {
    Pop-Location
}

$elf = Resolve-ExamplePath -Path $elfPath
$logDir = Split-Path -Parent $elf
$outFile = Join-Path $logDir "qemu-interrupt-sgi-timeout.log"
$errFile = Join-Path $logDir "qemu-interrupt-sgi-timeout.err.log"

Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue

$args = @(
    "-machine", "virt",
    "-cpu", "cortex-a7",
    "-nographic",
    "-monitor", "none",
    "-device", "loader,file=$elf,cpu-num=0"
)

$proc = Start-Process -FilePath $qemu -ArgumentList $args `
    -RedirectStandardOutput $outFile -RedirectStandardError $errFile -PassThru

$expected = @(
    "Charm ARMv7-A QEMU skeleton",
    "Targeting Cortex-A7 first, RK3506 later.",
    "ARMv7-A phase, stage=sgi-fiq-smoke",
    "ARMv7-A phase complete, stage=sgi-fiq-smoke",
    "ARMv7-A phase, stage=sgi-irq-timeout-smoke",
    "ARMv7-A phase complete, stage=sgi-irq-timeout-smoke",
    "ARMv7-A phase, stage=idle"
)

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
}

$log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
$missing = $expected | Where-Object { -not $log.Contains($_) }
$phaseMarker = "ARMv7-A phase, stage=sgi-irq-timeout-smoke"
$phaseIndex = $log.IndexOf($phaseMarker)
$timeoutLog = if ($phaseIndex -ge 0) { $log.Substring($phaseIndex) } else { $log }
if (($log -notmatch "ARMv7-A SGI pending evidence, route=irq, line=group[01]/(yes|no)/(yes|no)/(yes|no), gicd=0x[0-9A-F]{8}, gicc=0x[0-9A-F]{8}, hppir=0x[0-9A-F]{8}, spurious=no")) {
    $missing += "ARMv7-A SGI pending evidence, route=irq..."
}
if (($log -notmatch "ARMv7-A diagnostic context, subsystem=interrupt, stage=sgi-irq-timeout-smoke, last-complete=sgi-fiq-smoke, cpsr=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A diagnostic context, subsystem=interrupt, stage=sgi-irq-timeout-smoke..."
}
if (($log -notmatch "ARMv7-A interrupt timeout, expected=sgi-irq, route=irq, route-mask=masked, pending-observed=yes, last-observation=not-observed")) {
    $missing += "ARMv7-A interrupt timeout, expected=sgi-irq..."
}
if (($log -notmatch "ARMv7-A SGI timeout, igroupr0=0x[0-9A-F]{8}, isenabler0=0x[0-9A-F]{8}, ispendr0=0x[0-9A-F]{8}, isactiver0=0x[0-9A-F]{8}, hppir=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A SGI timeout, igroupr0=0x..."
}

$unexpected = @()
if ($timeoutLog.Contains("ARMv7-A SGI active")) {
    $unexpected += "ARMv7-A SGI active"
}
if ($timeoutLog.Contains("ARMv7-A unexpected IRQ")) {
    $unexpected += "ARMv7-A unexpected IRQ"
}
if ($timeoutLog.Contains("ARMv7-A special IRQ acknowledge")) {
    $unexpected += "ARMv7-A special IRQ acknowledge"
}

if ($missing.Count -gt 0 -or $unexpected.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines

    $details = @()
    if ($missing.Count -gt 0) {
        $details += "missing expected output: $($missing -join '; ')"
    }
    if ($unexpected.Count -gt 0) {
        $details += "unexpected output during SGI timeout smoke: $($unexpected -join '; ')"
    }

    throw ($details -join ' | ')
}

Write-Output "[ok] armv7a qemu SGI timeout smoke detected"
