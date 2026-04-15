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

    Write-Output "[armv7a-qemu-interrupt-special] log tail:"
    if (Test-Path $OutPath) {
        Get-Content $OutPath -Tail $Lines
    }
    if (Test-Path $ErrPath) {
        Get-Content $ErrPath -Tail $Lines
    }
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$configurePreset = "debug-interrupt-special-irq"
$buildPreset = "debug-interrupt-special-irq"
$elfPath = "out\\build\\debug-interrupt-special-irq\\charm-armv7a-qemu"

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
$outFile = Join-Path $logDir "qemu-interrupt-special.log"
$errFile = Join-Path $logDir "qemu-interrupt-special.err.log"

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
    "ARMv7-A phase, stage=special-irq-smoke",
    "ARMv7-A phase complete, stage=special-irq-smoke",
    "ARMv7-A phase, stage=idle"
)

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
}

$log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
$missing = $expected | Where-Object { -not $log.Contains($_) }
if (($log -notmatch "ARMv7-A diagnostic context, subsystem=interrupt, stage=special-irq-smoke, last-complete=sgi-fiq-smoke, cpsr=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A diagnostic context, subsystem=interrupt, stage=special-irq-smoke..."
}
if (($log -notmatch "ARMv7-A special IRQ acknowledge, intid=1023, source=special-intid, ack=0x[0-9A-F]{8}, hppir-before-ack=0x[0-9A-F]{8}, route=irq, origin-mode=sys, current-mode=sys, return-pc=0x40000000, synthetic=yes")) {
    $missing += "ARMv7-A special IRQ acknowledge, intid=1023..."
}
if (($log -notmatch "ARMv7-A security side evidence, scr-read=skipped, timer-source=(secure|non-secure)-phys-ppi/group[01], irq-source=self-sgi/group1, irq-origin=[a-z]+, irq-handler=irq, fiq-source=self-sgi/group0, fiq-origin=[a-z]+, fiq-handler=fiq, monitor-mode=(observed|not-observed)")) {
    $missing += "ARMv7-A security side evidence, scr-read=skipped..."
}

$unexpected = @()
if ($log.Contains("ARMv7-A timer IRQ timeout")) {
    $unexpected += "ARMv7-A timer IRQ timeout"
}
if ($log.Contains("ARMv7-A SGI timeout")) {
    $unexpected += "ARMv7-A SGI timeout"
}
if ($log.Contains("ARMv7-A FIQ timeout")) {
    $unexpected += "ARMv7-A FIQ timeout"
}

if ($missing.Count -gt 0 -or $unexpected.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines

    $details = @()
    if ($missing.Count -gt 0) {
        $details += "missing expected output: $($missing -join '; ')"
    }
    if ($unexpected.Count -gt 0) {
        $details += "unexpected output during special interrupt smoke: $($unexpected -join '; ')"
    }

    throw ($details -join ' | ')
}

Write-Output "[ok] armv7a qemu special interrupt smoke detected"
