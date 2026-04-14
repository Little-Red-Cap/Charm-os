param(
    [string]$QemuExe = "qemu-system-arm",
    [string]$ElfPath = "out\\build\\debug\\charm-armv7a-qemu",
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

$qemu = Resolve-ToolPath -Tool $QemuExe
$elf = Resolve-ExamplePath -Path $ElfPath
$outFile = Join-Path $PSScriptRoot "qemu-ci.log"
$errFile = Join-Path $PSScriptRoot "qemu-ci.err.log"

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
    "ARMv7-A SVC vector active, imm=0x000043"
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
if (($log -notmatch "ARMv7-A cp15 state, sctlr=0x[0-9A-F]{8}, vbar=0x[0-9A-F]{8}, mpidr=0x[0-9A-F]{8}, cntfrq=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A cp15 state, sctlr=0x..."
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
if (($log -notmatch "ARMv7-A small-page alias ready, va=0x5[0-9A-F]{7}, pa=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A small-page alias ready, va=0x..."
}
if (($log -notmatch "ARMv7-A MMU active, sctlr=0x[0-9A-F]{8}, ttbr0=0x[0-9A-F]{8}, ttbcr=0x[0-9A-F]{8}, dacr=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A MMU active, sctlr=0x..."
}
if (($log -notmatch "ARMv7-A MMU flags, mmu=on, dcache=(on|off), icache=(on|off)")) {
    $missing += "ARMv7-A MMU flags, mmu=on..."
}
if (($log -notmatch "ARMv7-A small-page probe, addr=0x5[0-9A-F]{7}, before=0xC0DEF00D, via-alias=0x1BADB002, direct=0x1BADB002")) {
    $missing += "ARMv7-A small-page probe, addr=0x..."
}
if (($log -notmatch "ARMv7-A timer IRQ active, intid=(29|30)")) {
    $missing += "ARMv7-A timer IRQ active, intid=29|30"
}
if ($missing.Count -gt 0) {
    Write-Output "[armv7a-qemu] log tail:"
    if (Test-Path $outFile) {
        Get-Content $outFile -Tail $TailLines
    }
    if (Test-Path $errFile) {
        Get-Content $errFile -Tail $TailLines
    }
    throw "missing expected output: $($missing -join '; ')"
}

Write-Output "[ok] armv7a qemu smoke detected"
