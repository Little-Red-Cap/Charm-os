param(
    [ValidateSet("data", "data-align", "prefetch", "prefetch-xn", "prefetch-page", "prefetch-page-xn", "prefetch-page-xn-runtime", "data-perm", "data-page", "data-page-perm", "data-page-perm-runtime")]
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
$extraPatterns = @()
$stackVector = ""

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
        $stackVector = "data-abort"
    }
    "data-align" {
        $configurePreset = "debug-abort-data-align"
        $buildPreset = "debug-abort-data-align"
        $elfPath = "out\\build\\debug-abort-data-align\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: data abort"
        $faultPattern = "ARMv7-A data fault, dfsr=0x[0-9A-F]{8}, dfar=0x4[0-9A-F]{7}, adfsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A data fault decode, status=0x01 \(alignment exception\), domain=0x[0-9A-F], write=no, cm=no"
        $mapPattern = "ARMv7-A fault map, far=0x4[0-9A-F]{7}, ttbr0=0x[0-9A-F]{8}, l1\[0x40[0-9A-F]\]=0x[0-9A-F]{8} \(section\), domain=0x0, xn=no, s=yes, c=yes, b=yes"
        $smokePattern = "ARMv7-A abort smoke, kind=data-align, addr=0x4[0-9A-F]{7}"
        $extraPatterns += "ARMv7-A data-align target ready, addr=0x4[0-9A-F]{7}, base=0x4[0-9A-F]{7}, value=0x89ABCDEF"
        $extraPatterns += "ARMv7-A alignment trap armed, addr=0x4[0-9A-F]{7}, base=0x4[0-9A-F]{7}, sctlr=0x[0-9A-F]{8}, alignment-check=on"
        $stackVector = "data-abort"
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
        $stackVector = "prefetch-abort"
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
        $extraPatterns += "ARMv7-A XN alias ready, va=0x5[0-9A-F]{7}, pa=0x4[0-9A-F]{7}, desc=0x[0-9A-F]{8}"
        $stackVector = "prefetch-abort"
    }
    "prefetch-page" {
        $configurePreset = "debug-abort-prefetch-page"
        $buildPreset = "debug-abort-prefetch-page"
        $elfPath = "out\\build\\debug-abort-prefetch-page\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: prefetch abort"
        $faultPattern = "ARMv7-A prefetch fault, ifsr=0x[0-9A-F]{8}, ifar=0x56[0-9A-F]{6}, aifsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A prefetch fault decode, status=0x07 \(page translation fault\), domain=0x0"
        $mapPattern = "ARMv7-A fault map, far=0x56[0-9A-F]{6}, ttbr0=0x[0-9A-F]{8}, l1\[0x560\]=0x[0-9A-F]{8} \(page table\), domain=0x0, l2\[0x00\]=0x00000000 \(fault\)"
        $smokePattern = "ARMv7-A abort smoke, kind=prefetch-page, addr=0x56[0-9A-F]{6}"
        $extraPatterns += "ARMv7-A prefetch-page alias ready, va=0x56[0-9A-F]{6}, l1=0x[0-9A-F]{8}, l2=0x00000000"
        $stackVector = "prefetch-abort"
    }
    "prefetch-page-xn" {
        $configurePreset = "debug-abort-prefetch-page-xn"
        $buildPreset = "debug-abort-prefetch-page-xn"
        $elfPath = "out\\build\\debug-abort-prefetch-page-xn\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: prefetch abort"
        $faultPattern = "ARMv7-A prefetch fault, ifsr=0x[0-9A-F]{8}, ifar=0x55[0-9A-F]{6}, aifsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A prefetch fault decode, status=0x0F \(page permission fault\), domain=0x1"
        $mapPattern = "ARMv7-A fault map, far=0x55[0-9A-F]{6}, ttbr0=0x[0-9A-F]{8}, l1\[0x550\]=0x[0-9A-F]{8} \(page table\), domain=0x1, l2\[0x00\]=0x[0-9A-F]{8} \(small page\), xn=yes, s=yes, c=yes, b=yes"
        $smokePattern = "ARMv7-A abort smoke, kind=prefetch-page-xn, addr=0x55[0-9A-F]{6}"
        $extraPatterns += "ARMv7-A page-XN alias ready, va=0x55[0-9A-F]{6}, pa=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}"
        $stackVector = "prefetch-abort"
    }
    "prefetch-page-xn-runtime" {
        $configurePreset = "debug-abort-prefetch-page-xn-runtime"
        $buildPreset = "debug-abort-prefetch-page-xn-runtime"
        $elfPath = "out\\build\\debug-abort-prefetch-page-xn-runtime\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: prefetch abort"
        $faultPattern = "ARMv7-A prefetch fault, ifsr=0x[0-9A-F]{8}, ifar=0x58[0-9A-F]{6}, aifsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A prefetch fault decode, status=0x0F \(page permission fault\), domain=0x1"
        $mapPattern = "ARMv7-A fault map, far=0x58[0-9A-F]{6}, ttbr0=0x[0-9A-F]{8}, l1\[0x580\]=0x[0-9A-F]{8} \(page table\), domain=0x1, l2\[0x00\]=0x[0-9A-F]{8} \(small page\), xn=yes, s=yes, c=yes, b=yes"
        $smokePattern = "ARMv7-A abort smoke, kind=prefetch-page-xn-runtime, addr=0x58[0-9A-F]{6}"
        $extraPatterns += "ARMv7-A runtime page-XN alias ready, va=0x58[0-9A-F]{6}, pa=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}"
        $extraPatterns += "ARMv7-A runtime page-XN probe, addr=0x58[0-9A-F]{6}, return=0x00000043"
        $extraPatterns += "ARMv7-A runtime page-XN flip, addr=0x58[0-9A-F]{6}, l2=0x[0-9A-F]{8}"
        $stackVector = "prefetch-abort"
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
        $extraPatterns += "ARMv7-A data alias ready, va=0x5[0-9A-F]{7}, pa=0x4[0-9A-F]{7}, desc=0x[0-9A-F]{8}"
        $stackVector = "data-abort"
    }
    "data-page" {
        $configurePreset = "debug-abort-data-page"
        $buildPreset = "debug-abort-data-page"
        $elfPath = "out\\build\\debug-abort-data-page\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: data abort"
        $faultPattern = "ARMv7-A data fault, dfsr=0x[0-9A-F]{8}, dfar=0x53000040, adfsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A data fault decode, status=0x07 \(page translation fault\), domain=0x0, write=no, cm=no"
        $mapPattern = "ARMv7-A fault map, far=0x53000040, ttbr0=0x[0-9A-F]{8}, l1\[0x530\]=0x[0-9A-F]{8} \(page table\), domain=0x0, l2\[0x00\]=0x00000000 \(fault\)"
        $smokePattern = "ARMv7-A abort smoke, kind=data-page, addr=0x53000040"
        $extraPatterns += "ARMv7-A data-page alias ready, va=0x53000040, l1=0x[0-9A-F]{8}, l2=0x00000000"
        $stackVector = "data-abort"
    }
    "data-page-perm" {
        $configurePreset = "debug-abort-data-page-perm"
        $buildPreset = "debug-abort-data-page-perm"
        $elfPath = "out\\build\\debug-abort-data-page-perm\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: data abort"
        $faultPattern = "ARMv7-A data fault, dfsr=0x[0-9A-F]{8}, dfar=0x54[0-9A-F]{6}, adfsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A data fault decode, status=0x0F \(page permission fault\), domain=0x1, write=yes, cm=no"
        $mapPattern = "ARMv7-A fault map, far=0x54[0-9A-F]{6}, ttbr0=0x[0-9A-F]{8}, l1\[0x540\]=0x[0-9A-F]{8} \(page table\), domain=0x1, l2\[0x00\]=0x[0-9A-F]{8} \(small page\), xn=yes, s=yes, c=yes, b=yes"
        $smokePattern = "ARMv7-A abort smoke, kind=data-page-perm, addr=0x54[0-9A-F]{6}, value=0xA5A55A5A"
        $extraPatterns += "ARMv7-A data-page-perm alias ready, va=0x54[0-9A-F]{6}, pa=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}"
        $stackVector = "data-abort"
    }
    "data-page-perm-runtime" {
        $configurePreset = "debug-abort-data-page-perm-runtime"
        $buildPreset = "debug-abort-data-page-perm-runtime"
        $elfPath = "out\\build\\debug-abort-data-page-perm-runtime\\charm-armv7a-qemu"
        $exceptionLine = "ARMv7-A exception: data abort"
        $faultPattern = "ARMv7-A data fault, dfsr=0x[0-9A-F]{8}, dfar=0x57[0-9A-F]{6}, adfsr=0x[0-9A-F]{8}"
        $decodePattern = "ARMv7-A data fault decode, status=0x0F \(page permission fault\), domain=0x1, write=yes, cm=no"
        $mapPattern = "ARMv7-A fault map, far=0x57[0-9A-F]{6}, ttbr0=0x[0-9A-F]{8}, l1\[0x570\]=0x[0-9A-F]{8} \(page table\), domain=0x1, l2\[0x00\]=0x[0-9A-F]{8} \(small page\), xn=yes, s=yes, c=yes, b=yes"
        $smokePattern = "ARMv7-A abort smoke, kind=data-page-perm-runtime, addr=0x57[0-9A-F]{6}, value=0xA5A55A5A"
        $extraPatterns += "ARMv7-A runtime data-page alias ready, va=0x57[0-9A-F]{6}, pa=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}"
        $extraPatterns += "ARMv7-A runtime data-page probe, addr=0x57[0-9A-F]{6}, before=0x0BADCAFE, after=0x5AA55AA5, direct=0x5AA55AA5"
        $extraPatterns += "ARMv7-A runtime data-page flip, addr=0x57[0-9A-F]{6}, l2=0x[0-9A-F]{8}"
        $stackVector = "data-abort"
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
    "ARMv7-A phase, stage=boot-cpu-state",
    "ARMv7-A phase complete, stage=boot-cpu-state",
    "ARMv7-A phase, stage=mmu-activate",
    "ARMv7-A phase complete, stage=mmu-activate",
    "ARMv7-A phase, stage=abort-smoke",
    "ARMv7-A phase complete, stage=icache-probe",
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
if (($log -notmatch "ARMv7-A attr probe ready, va=0x52300000, pa=0x4[0-9A-F]{7}, section=0x4[0-9A-F]{7}, identity-l1=0x00000000, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}, tex=0x00000001, mem=normal-cached")) {
    $missing += "ARMv7-A attr probe ready, va=0x52300000..."
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
if (($log -notmatch "ARMv7-A attr probe, addr=0x52300000, before=0x11223344, normal=0x55667788, device-before=0x55667788, device=0x99AABBCC, restored=0x99AABBCC")) {
    $missing += "ARMv7-A attr probe, addr=0x52300000..."
}
if (($log -notmatch "ARMv7-A attr descriptors, normal=0x[0-9A-F]{8} .*mem=normal-cached.*device=0x[0-9A-F]{8} .*mem=device.*restored=0x[0-9A-F]{8} .*mem=normal-cached")) {
    $missing += "ARMv7-A attr descriptors, normal=0x..."
}
if (($log -notmatch "ARMv7-A icache probe, addr=0x522[0-9A-F]{5}, before=0x000000A1, after=0x000000B2, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A icache probe, addr=0x522..."
}
if (($log -notmatch $faultPattern)) {
    $missing += $faultPattern
}
if (($log -notmatch "ARMv7-A exception phase, stage=abort-smoke, last-complete=icache-probe")) {
    $missing += "ARMv7-A exception phase, stage=abort-smoke, last-complete=icache-probe"
}
if (($log -notmatch $decodePattern)) {
    $missing += $decodePattern
}
if (($log -notmatch $mapPattern)) {
    $missing += $mapPattern
}
if (($log -notmatch "ARMv7-A handler stack, vector=$stackVector, mode=abt, sp=0x[0-9A-F]{8}, base=0x[0-9A-F]{8}, top=0x[0-9A-F]{8}, used=0x[0-9A-F]{8}, in-range=yes")) {
    $missing += "ARMv7-A handler stack, vector=$stackVector..."
}
foreach ($extraPattern in $extraPatterns) {
    if ($log -notmatch $extraPattern) {
        $missing += $extraPattern
    }
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
if ($log.Contains("ARMv7-A SGI active")) {
    $unexpected += "ARMv7-A SGI active"
}
if ($log.Contains("ARMv7-A FIQ active")) {
    $unexpected += "ARMv7-A FIQ active"
}
if ($log.Contains("ARMv7-A security side evidence")) {
    $unexpected += "ARMv7-A security side evidence"
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
