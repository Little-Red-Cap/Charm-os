param(
    [ValidateSet("data", "prefetch", "prefetch-xn", "data-perm")]
    [string]$Kind = "data",
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

    Write-Output "[armv7a-qemu-abort] log tail:"
    if (Test-Path $OutPath) {
        Get-Content $OutPath -Tail $Lines
    }
    if (Test-Path $ErrPath) {
        Get-Content $ErrPath -Tail $Lines
    }
}

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe
$extraPattern = $null

switch ($Kind) {
    "data" {
        $configurePreset = "debug-abort-data"
        $buildPreset = "debug-abort-data"
        $elfPath = "out\\build\\debug-abort-data\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: data abort"
        $faultPattern = "ARMv7-A data fault, dfsr=0x[0-9A-F]{8}, dfar=0x20000000, adfsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A data fault decode, status=0x05 \(section translation fault\), domain=0x0, write=no, cm=no"
        $mapPattern = "ARMv7-A fault map, far=0x20000000, ttbr0=0x[0-9A-F]{8}, l1\[0x200\]=0x00000000 \(fault\)"
        $smokePattern = "ARMv7-A abort smoke, kind=data, addr=0x20000000"
    }
    "prefetch" {
        $configurePreset = "debug-abort-prefetch"
        $buildPreset = "debug-abort-prefetch"
        $elfPath = "out\\build\\debug-abort-prefetch\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: prefetch abort"
        $faultPattern = "ARMv7-A prefetch fault, ifsr=0x[0-9A-F]{8}, ifar=0x20000000, aifsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A prefetch fault decode, status=0x05 \(section translation fault\), domain=0x0"
        $mapPattern = "ARMv7-A fault map, far=0x20000000, ttbr0=0x[0-9A-F]{8}, l1\[0x200\]=0x00000000 \(fault\)"
        $smokePattern = "ARMv7-A abort smoke, kind=prefetch, addr=0x20000000"
    }
    "prefetch-xn" {
        $configurePreset = "debug-abort-prefetch-xn"
        $buildPreset = "debug-abort-prefetch-xn"
        $elfPath = "out\\build\\debug-abort-prefetch-xn\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: prefetch abort"
        $faultPattern = "ARMv7-A prefetch fault, ifsr=0x[0-9A-F]{8}, ifar=0x5[0-9A-F]{7}, aifsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A prefetch fault decode, status=0x0D \(section permission fault\), domain=0x1"
        $mapPattern = "ARMv7-A fault map, far=0x5[0-9A-F]{7}, ttbr0=0x[0-9A-F]{8}, l1\[0x500\]=0x[0-9A-F]{8} \(section\), domain=0x1, xn=yes, s=yes, c=yes, b=yes"
        $smokePattern = "ARMv7-A abort smoke, kind=prefetch-xn, addr=0x5[0-9A-F]{7}"
        $extraPattern = "ARMv7-A XN alias ready, va=0x5[0-9A-F]{7}, pa=0x4[0-9A-F]{7}, desc=0x[0-9A-F]{8}"
    }
    "data-perm" {
        $configurePreset = "debug-abort-data-perm"
        $buildPreset = "debug-abort-data-perm"
        $elfPath = "out\\build\\debug-abort-data-perm\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: data abort"
        $faultPattern = "ARMv7-A data fault, dfsr=0x[0-9A-F]{8}, dfar=0x5[0-9A-F]{7}, adfsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A data fault decode, status=0x0D \(section permission fault\), domain=0x1, write=yes, cm=no"
        $mapPattern = "ARMv7-A fault map, far=0x5[0-9A-F]{7}, ttbr0=0x[0-9A-F]{8}, l1\[0x510\]=0x[0-9A-F]{8} \(section\), domain=0x1, xn=yes, s=yes, c=yes, b=yes"
        $smokePattern = "ARMv7-A abort smoke, kind=data-perm, addr=0x5[0-9A-F]{7}, value=0xA5A55A5A"
        $extraPattern = "ARMv7-A data alias ready, va=0x5[0-9A-F]{7}, pa=0x4[0-9A-F]{7}, desc=0x[0-9A-F]{8}"
    }
    default {
        throw "unsupported abort kind: $Kind"
    }
}

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
$outFile = Join-Path $logDir "qemu-abort-$Kind.log"
$errFile = Join-Path $logDir "qemu-abort-$Kind.err.log"

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
    $exceptionLine
)

Start-Sleep -Seconds $TimeoutSec

if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Wait-Process -Id $proc.Id -Timeout 2 -ErrorAction SilentlyContinue
}

$log = (Read-LogSafe -Path $outFile) + (Read-LogSafe -Path $errFile)
$missing = $expected | Where-Object { -not $log.Contains($_) }
if (($log -notmatch $smokePattern)) {
    $missing += $smokePattern
}

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
if (($log -notmatch "ARMv7-A MMU active, sctlr=0x[0-9A-F]{8}, ttbr0=0x[0-9A-F]{8}, ttbcr=0x[0-9A-F]{8}, dacr=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A MMU active, sctlr=0x..."
}
if (($log -notmatch "ARMv7-A MMU flags, mmu=on, dcache=(on|off), icache=(on|off)")) {
    $missing += "ARMv7-A MMU flags, mmu=on..."
}
if (($log -notmatch $faultPattern)) {
    $missing += $faultPattern
}
if (($log -notmatch $decodePattern)) {
    $missing += $decodePattern
}
if (($log -notmatch $mapPattern)) {
    $missing += $mapPattern
}
if ($extraPattern -and ($log -notmatch $extraPattern)) {
    $missing += $extraPattern
}
if (($log -notmatch "ARMv7-A fault context, sctlr=0x[0-9A-F]{8}, ttbr0=0x[0-9A-F]{8}, ttbcr=0x[0-9A-F]{8}, dacr=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A fault context, sctlr=0x..."
}

$unexpected = @()
if ($log.Contains("ARMv7-A abort smoke unexpectedly returned")) {
    $unexpected += "ARMv7-A abort smoke unexpectedly returned"
}
if ($log.Contains("ARMv7-A SVC vector active")) {
    $unexpected += "ARMv7-A SVC vector active"
}
if ($log.Contains("ARMv7-A timer IRQ active")) {
    $unexpected += "ARMv7-A timer IRQ active"
}

if ($missing.Count -gt 0 -or $unexpected.Count -gt 0) {
    Show-LogTail -OutPath $outFile -ErrPath $errFile -Lines $TailLines

    $details = @()
    if ($missing.Count -gt 0) {
        $details += "missing expected output: $($missing -join '; ')"
    }
    if ($unexpected.Count -gt 0) {
        $details += "unexpected output after abort smoke: $($unexpected -join '; ')"
    }

    throw ($details -join ' | ')
}

Write-Output "[ok] armv7a qemu abort smoke detected ($Kind)"
