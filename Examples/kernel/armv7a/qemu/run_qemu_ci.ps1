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
if (($log -notmatch "ARMv7-A small-page remap ready, va=0x52100000, pa-a=0x4[0-9A-F]{7}, pa-b=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A small-page remap ready, va=0x52100000..."
}
if (($log -notmatch "ARMv7-A attr probe ready, va=0x52300000, pa=0x4[0-9A-F]{7}, section=0x4[0-9A-F]{7}, identity-l1=0x00000000, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}, tex=0x00000001, mem=normal-cached")) {
    $missing += "ARMv7-A attr probe ready, va=0x52300000..."
}
if (($log -notmatch "ARMv7-A dcache probe ready, va=0x52400000, pa=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A dcache probe ready, va=0x52400000..."
}
if (($log -notmatch "ARMv7-A MMU active, sctlr=0x[0-9A-F]{8}, ttbr0=0x[0-9A-F]{8}, ttbcr=0x[0-9A-F]{8}, dacr=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A MMU active, sctlr=0x..."
}
if (($log -notmatch "ARMv7-A icache probe ready, va=0x522[0-9A-F]{5}, pa-a=0x4[0-9A-F]{7}, pa-b=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A icache probe ready, va=0x522..."
}
if (($log -notmatch "ARMv7-A MMU flags, mmu=on, dcache=off, icache=on")) {
    $missing += "ARMv7-A MMU flags, mmu=on, dcache=off, icache=on"
}
if (($log -notmatch "ARMv7-A small-page probe, addr=0x5[0-9A-F]{7}, before=0xC0DEF00D, via-alias=0x1BADB002, direct=0x1BADB002")) {
    $missing += "ARMv7-A small-page probe, addr=0x..."
}
if (($log -notmatch "ARMv7-A small-page remap, addr=0x52100000, before=0x13579BDF, after=0x2468ACE0, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A small-page remap, addr=0x52100000..."
}
if (($log -notmatch "ARMv7-A attr probe, addr=0x52300000, before=0x11223344, normal=0x55667788, device-before=0x55667788, device=0x99AABBCC, restored=0x99AABBCC")) {
    $missing += "ARMv7-A attr probe, addr=0x52300000..."
}
if (($log -notmatch "ARMv7-A attr descriptors, normal=0x[0-9A-F]{8} .*mem=normal-cached.*device=0x[0-9A-F]{8} .*mem=device.*restored=0x[0-9A-F]{8} .*mem=normal-cached")) {
    $missing += "ARMv7-A attr descriptors, normal=0x..."
}
if (($log -notmatch "ARMv7-A icache probe, addr=0x522[0-9A-F]{5}, before=0x000000A1, after=0x000000B2, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A icache probe, addr=0x522..."
}
if (($log -notmatch "ARMv7-A D-cache active, sctlr=0x[0-9A-F]{8}, clidr=0x[0-9A-F]{8}, ccsidr=0x[0-9A-F]{8}, line=0x[0-9A-F]{8}, ways=0x[0-9A-F]{8}, sets=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A D-cache active, sctlr=0x..."
}
if (($log -notmatch "ARMv7-A dcache probe, addr=0x52400000, before=0xCAFEBABE, cached=0x10203040, device-before=0x10203040, restored=0x50607080, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A dcache probe, addr=0x52400000..."
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
