param(
    [ValidateSet("undefined")]
    [string]$Kind = "undefined",
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

    Write-Output "[armv7a-qemu-exception] log tail:"
    if (Test-Path $OutPath) {
        Get-Content $OutPath -Tail $Lines
    }
    if (Test-Path $ErrPath) {
        Get-Content $ErrPath -Tail $Lines
    }
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$configurePreset = "debug-exception-undefined"
$buildPreset = "debug-exception-undefined"
$elfPath = "out\\build\\debug-exception-undefined\\charm-armv7a-qemu"

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
$outFile = Join-Path $logDir "qemu-exception-$Kind.log"
$errFile = Join-Path $logDir "qemu-exception-$Kind.err.log"

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
    "Charm out.format import active, PL011 @ 0x09000000",
    "ARMv7-A exception smoke, kind=undefined"
)

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
}

$log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
$missing = $expected | Where-Object { -not $log.Contains($_) }
if (($log -notmatch "ARMv7-A boot state, cpsr=0x[0-9A-F]{8}, mode=[a-z]+, irq=(masked|enabled)")) {
    $missing += "ARMv7-A boot state, cpsr=0x..."
}
if (($log -notmatch "ARMv7-A reset evidence, sctlr=0x[0-9A-F]{8}, vbar=0x[0-9A-F]{8}, high-vectors=(on|off), low-vectors-forced=(yes|no)")) {
    $missing += "ARMv7-A reset evidence, sctlr=0x..."
}
if (($log -notmatch "ARMv7-A cp15 state, sctlr=0x[0-9A-F]{8}, vbar=0x[0-9A-F]{8}, mpidr=0x[0-9A-F]{8}, cntfrq=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A cp15 state, sctlr=0x..."
}
if (($log -notmatch "ARMv7-A interrupt reset state, gicd=0x[0-9A-F]{8}, gicc=0x[0-9A-F]{8}, pmr=0x[0-9A-F]{8}, bpr=0x[0-9A-F]{8}, hppir=0x[0-9A-F]{8}, spurious=(yes|no)")) {
    $missing += "ARMv7-A interrupt reset state, gicd=0x..."
}
if (($log -notmatch "ARMv7-A timer reset state, cntp_ctl=0x[0-9A-F]{8}, enabled=(yes|no), imask=(yes|no), istatus=(yes|no), secure-line=group[01]/(yes|no)/(yes|no)/(yes|no), nonsecure-line=group[01]/(yes|no)/(yes|no)/(yes|no)")) {
    $missing += "ARMv7-A timer reset state, cntp_ctl=0x..."
}
if (($log -notmatch "ARMv7-A SGI reset state, line=group[01]/(yes|no)/(yes|no)/(yes|no)")) {
    $missing += "ARMv7-A SGI reset state, line=group..."
}
if (($log -notmatch "ARMv7-A memory model, id_mmfr0=0x[0-9A-F]{8}, vmsa=0x[0-9A-F]{8} \((present|absent)\), pmsa=0x[0-9A-F]{8} \((present|absent)\)")) {
    $missing += "ARMv7-A memory model, id_mmfr0=0x..."
}
if (($log -notmatch "ARMv7-A feature state, id_pfr1=0x[0-9A-F]{8}, security=0x[0-9A-F]{8} \((present|absent)\), virtualization=0x[0-9A-F]{8} \((present|absent)\), gentimer=0x[0-9A-F]{8} \((present|absent)\)")) {
    $missing += "ARMv7-A feature state, id_pfr1=0x..."
}
if (($log -notmatch "ARMv7-A cache state, mmu=(on|off), dcache=(on|off), icache=(on|off), high-vectors=(on|off)")) {
    $missing += "ARMv7-A cache state, mmu=..."
}
if (($log -notmatch "ARMv7-A translation state, ttbr0=0x[0-9A-F]{8}, ttbr1=0x[0-9A-F]{8}, ttbcr=0x[0-9A-F]{8}, dacr=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A translation state, ttbr0=0x..."
}
if (($log -notmatch "ARMv7-A L1 table ready, base=0x[0-9A-F]{8}, ram=0x[0-9A-F]{8}, gic=0x[0-9A-F]{8}, uart=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A L1 table ready, base=0x..."
}
if (($log -notmatch "ARMv7-A MMU active, sctlr=0x[0-9A-F]{8}, ttbr0=0x[0-9A-F]{8}, ttbcr=0x[0-9A-F]{8}, dacr=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A MMU active, sctlr=0x..."
}
if (($log -notmatch "ARMv7-A MMU flags, mmu=on, dcache=off, icache=on")) {
    $missing += "ARMv7-A MMU flags, mmu=on, dcache=off, icache=on"
}
if (($log -notmatch "ARMv7-A exception: undefined, pc=0x[0-9A-F]{8}, lr=0x[0-9A-F]{8}, spsr=0x[0-9A-F]{8}, origin-mode=[a-z]+, current-cpsr=0x[0-9A-F]{8}, current-mode=und")) {
    $missing += "ARMv7-A exception: undefined, pc=0x..."
}

$unexpected = @()
if ($log.Contains("ARMv7-A exception smoke unexpectedly returned")) {
    $unexpected += "ARMv7-A exception smoke unexpectedly returned"
}
if ($log.Contains("ARMv7-A SVC vector active")) {
    $unexpected += "ARMv7-A SVC vector active"
}
if ($log.Contains("ARMv7-A timer IRQ active")) {
    $unexpected += "ARMv7-A timer IRQ active"
}
if ($log.Contains("ARMv7-A SGI active")) {
    $unexpected += "ARMv7-A SGI active"
}
if ($log.Contains("ARMv7-A FIQ active")) {
    $unexpected += "ARMv7-A FIQ active"
}
if ($log.Contains("ARMv7-A security side evidence")) {
    $unexpected += "ARMv7-A security side evidence"
}
if ($log.Contains("ARMv7-A D-cache active")) {
    $unexpected += "ARMv7-A D-cache active"
}

if ($missing.Count -gt 0 -or $unexpected.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines

    $details = @()
    if ($missing.Count -gt 0) {
        $details += "missing expected output: $($missing -join '; ')"
    }
    if ($unexpected.Count -gt 0) {
        $details += "unexpected output after exception smoke: $($unexpected -join '; ')"
    }

    throw ($details -join ' | ')
}

Write-Output "[ok] armv7a qemu exception smoke detected ($Kind)"
