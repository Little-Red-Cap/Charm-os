param(
    [string]$CMakeExe = "cmake",
    [string]$QemuExe = "qemu-system-arm",
    [string]$ElfPath = "out\\build\\debug\\charm-armv7a-qemu",
    [int]$BuildJobs = 1,
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

$cmake = Resolve-ToolPath -Tool $CMakeExe
$qemu = Resolve-ToolPath -Tool $QemuExe

Push-Location $PSScriptRoot
try {
    & $cmake --preset debug
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed for preset: debug"
    }

    & $cmake --build --preset debug --parallel $BuildJobs
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed for preset: debug"
    }
} finally {
    Pop-Location
}

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
    "ARMv7-A phase, stage=boot-cpu-state",
    "ARMv7-A phase complete, stage=boot-cpu-state",
    "ARMv7-A phase, stage=memory-probe-prepare",
    "ARMv7-A phase complete, stage=memory-probe-prepare",
    "ARMv7-A phase, stage=memory-probe-describe",
    "ARMv7-A phase complete, stage=memory-probe-describe",
    "ARMv7-A phase, stage=mmu-activate",
    "ARMv7-A phase complete, stage=mmu-activate",
    "ARMv7-A phase, stage=small-page-probe",
    "ARMv7-A phase complete, stage=small-page-probe",
    "ARMv7-A phase, stage=attribute-probe",
    "ARMv7-A phase complete, stage=attribute-probe",
    "ARMv7-A phase, stage=icache-probe",
    "ARMv7-A phase complete, stage=icache-probe",
    "ARMv7-A phase, stage=abort-smoke",
    "ARMv7-A phase complete, stage=abort-smoke",
    "ARMv7-A phase, stage=exception-smoke",
    "ARMv7-A phase complete, stage=exception-smoke",
    "ARMv7-A phase, stage=dcache-probe",
    "ARMv7-A phase complete, stage=dcache-probe",
    "ARMv7-A phase, stage=page-table-probe",
    "ARMv7-A phase complete, stage=page-table-probe",
    "ARMv7-A phase, stage=section-split-probe",
    "ARMv7-A phase complete, stage=section-split-probe",
    "ARMv7-A phase, stage=svc-smoke",
    "ARMv7-A phase complete, stage=svc-smoke",
    "ARMv7-A phase, stage=timer-irq-smoke",
    "ARMv7-A phase complete, stage=timer-irq-smoke",
    "ARMv7-A phase, stage=sgi-irq-smoke",
    "ARMv7-A phase complete, stage=sgi-irq-smoke",
    "ARMv7-A phase, stage=sgi-fiq-smoke",
    "ARMv7-A phase complete, stage=sgi-fiq-smoke",
    "ARMv7-A phase, stage=kernel-ingress",
    "ARMv7-A phase complete, stage=kernel-ingress",
    "ARMv7-A phase, stage=scheduler-tick-ingress",
    "ARMv7-A phase complete, stage=scheduler-tick-ingress",
    "ARMv7-A phase, stage=runtime-trap-ingress",
    "ARMv7-A phase complete, stage=runtime-trap-ingress",
    "ARMv7-A phase, stage=context-switch-smoke",
    "ARMv7-A phase complete, stage=context-switch-smoke",
    "ARMv7-A phase, stage=scheduler-dispatch",
    "ARMv7-A phase complete, stage=scheduler-dispatch",
    "ARMv7-A phase, stage=runtime-bridge",
    "ARMv7-A phase complete, stage=runtime-bridge",
    "ARMv7-A phase, stage=handoff-prepare",
    "ARMv7-A phase complete, stage=handoff-prepare",
    "ARMv7-A phase, stage=idle",
    "ARMv7-A SVC vector active, imm=0x000043",
    "ARMv7-A SVC vector active, imm=0x000044"
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
if (($log -notmatch "ARMv7-A page-table probe ready, va=0x52500000, pa-a=0x4[0-9A-F]{7}, pa-b=0x4[0-9A-F]{7}, desc=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A page-table probe ready, va=0x52500000..."
}
if (($log -notmatch "ARMv7-A section-split probe ready, section=0x52600000, addr=0x526[0-9A-F]{5}, pa-section=0x4[0-9A-F]{7}, pa-a=0x4[0-9A-F]{7}, pa-b=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A section-split probe ready, section=0x52600000..."
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
if (($log -notmatch "ARMv7-A page-table probe, addr=0x52500000, before=0x31415926, after=0x27182818, restored=0x31415926, desc=0x4[0-9A-F]{7}, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A page-table probe, addr=0x52500000..."
}
if (($log -notmatch "ARMv7-A section-split probe, addr=0x526[0-9A-F]{5}, before=0x89ABCDEF, after=0x76543210, restored=0x89ABCDEF, l1-desc=0x4[0-9A-F]{7}, l2-table=0x4[0-9A-F]{7}, l1=0x[0-9A-F]{8}, l2=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A section-split probe, addr=0x526..."
}
if (($log -notmatch "ARMv7-A timer IRQ active, intid=(29|30), source=(secure|non-secure)-phys-ppi, ack=0x[0-9A-F]{8}, hppir-before-ack=0x[0-9A-F]{8}, line=group[01]/yes/(yes|no)/yes")) {
    $missing += "ARMv7-A timer IRQ active, intid=29|30, source=..."
}
if (($log -notmatch "ARMv7-A timer pending evidence, cntp_ctl=0x[0-9A-F]{8}, secure-line=group[01]/(yes|no)/(yes|no)/(yes|no), nonsecure-line=group[01]/(yes|no)/(yes|no)/(yes|no), gicd=0x[0-9A-F]{8}, gicc=0x[0-9A-F]{8}, hppir=0x[0-9A-F]{8}, spurious=no")) {
    $missing += "ARMv7-A timer pending evidence, cntp_ctl=0x..."
}
if (($log -notmatch "ARMv7-A SVC vector active, imm=0x000043, origin-mode=sys, handler-mode=svc, return-pc=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A SVC vector active, imm=0x000043, origin-mode=sys, handler-mode=svc, return-pc=0x..."
}
if (($log -notmatch "ARMv7-A SVC vector active, imm=0x000044, origin-mode=sys, handler-mode=svc, return-pc=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A SVC vector active, imm=0x000044, origin-mode=sys, handler-mode=svc, return-pc=0x..."
}
if (($log -notmatch "ARMv7-A handler stack, vector=svc, mode=svc, sp=0x[0-9A-F]{8}, base=0x[0-9A-F]{8}, top=0x[0-9A-F]{8}, used=0x[0-9A-F]{8}, in-range=yes")) {
    $missing += "ARMv7-A handler stack, vector=svc..."
}
if (($log -notmatch "ARMv7-A return evidence, vector=svc, origin-mode=sys, current-mode=sys, origin-irq=masked, current-irq=masked, origin-fiq=masked, current-fiq=masked, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x[0-9A-F]{8}, base=0x[0-9A-F]{8}, top=0x[0-9A-F]{8}, used=0x[0-9A-F]{8}, in-range=yes")) {
    $missing += "ARMv7-A return evidence, vector=svc..."
}
if (($log -notmatch "ARMv7-A timer IRQ active, intid=(29|30), source=(secure|non-secure)-phys-ppi, ack=0x[0-9A-F]{8}, hppir-before-ack=0x[0-9A-F]{8}, line=group[01]/yes/(yes|no)/yes, origin-mode=sys, handler-mode=irq, return-pc=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A timer IRQ active, intid=29|30, source=..., origin-mode=sys, handler-mode=irq, return-pc=0x..."
}
if (($log -notmatch "ARMv7-A timer IRQ complete, intid=(29|30), source=(secure|non-secure)-phys-ppi, eoi=0x[0-9A-F]{8}, hppir-after-eoi=0x[0-9A-F]{8}, line-after-eoi=group[01]/yes/(yes|no)/no, active-cleared=yes, controller-advanced=yes, retired=yes")) {
    $missing += "ARMv7-A timer IRQ complete, intid=29|30, source=..."
}
if (($log -notmatch "ARMv7-A timer IRQ lifecycle, intid=(29|30), source=(secure|non-secure)-phys-ppi, entry-match=yes, retired=yes, restored=yes, closed=yes")) {
    $missing += "ARMv7-A timer IRQ lifecycle, intid=29|30, source=..."
}
if (([regex]::Matches($log, "ARMv7-A return evidence, vector=irq, origin-mode=sys, current-mode=sys, origin-irq=enabled, current-irq=enabled, origin-fiq=masked, current-fiq=masked, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x[0-9A-F]{8}, base=0x[0-9A-F]{8}, top=0x[0-9A-F]{8}, used=0x[0-9A-F]{8}, in-range=yes")).Count -lt 2) {
    $missing += "ARMv7-A return evidence, vector=irq x2"
}
if (($log -notmatch "ARMv7-A SGI active, intid=1, source=self-sgi, ack=0x[0-9A-F]{8}, hppir-before-ack=0x[0-9A-F]{8}, line=group1/yes/(yes|no)/yes, origin-mode=sys, handler-mode=irq, return-pc=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A SGI active, intid=1, source=self-sgi, handler-mode=irq"
}
if (($log -notmatch "ARMv7-A SGI complete, intid=1, source=self-sgi, eoi=0x[0-9A-F]{8}, hppir-after-eoi=0x[0-9A-F]{8}, line-after-eoi=group1/yes/(yes|no)/no, active-cleared=yes, controller-advanced=yes, retired=yes")) {
    $missing += "ARMv7-A SGI complete, intid=1, source=self-sgi"
}
if (($log -notmatch "ARMv7-A SGI lifecycle, intid=1, source=self-sgi, entry-match=yes, retired=yes, restored=yes, closed=yes")) {
    $missing += "ARMv7-A SGI lifecycle, intid=1, source=self-sgi"
}
if (($log -notmatch "ARMv7-A kernel ingress, vector-base=0x[0-9A-F]{8}, tick-mode=oneshot, tick-route=irq, timer-hz=[0-9]+, exception=yes, interrupt=yes, timer=yes, context-ready=yes, context-model=software-frame, tick-runtime=yes, thread-runtime=yes")) {
    $missing += "ARMv7-A kernel ingress, vector-base=0x..."
}
if (($log -notmatch "ARMv7-A scheduler tick ingress, source=timer-irq, route=irq, mode=oneshot, intid=(29|30), hz=[0-9]+, now=0x[0-9A-F]{16}, source-match=yes, counter=yes, isr-safe=yes, retired=yes, handoff=yes, rearm=yes")) {
    $missing += "ARMv7-A scheduler tick ingress, source=timer-irq..."
}
if (($log -notmatch "ARMv7-A runtime trap ingress, source=svc, service=0x000043, arg0=0x00000001, arg1=0x00000001, arg2=0x00000000, arg3=0x00000000, service-ready=yes, args-ready=yes, trap=yes")) {
    $missing += "ARMv7-A runtime trap ingress, source=svc..."
}
if (($log -notmatch "ARMv7-A thread frame, kind=cooperative-sys, stack-base=0x[0-9A-F]{8}, stack-top=0x[0-9A-F]{8}, prepared-sp=0x[0-9A-F]{8}, resume=0x[0-9A-F]{8}, return=0x[0-9A-F]{8}, entry=0x[0-9A-F]{8}, arg=0x[0-9A-F]{8}, aligned=yes, in-range=yes, ready=yes")) {
    $missing += "ARMv7-A thread frame, kind=cooperative-sys..."
}
if (($log -notmatch "ARMv7-A context switch smoke, main-before=0x[0-9A-F]{8}, main-saved=0x[0-9A-F]{8}, thread-entry-sp=0x[0-9A-F]{8}, thread-saved=0x[0-9A-F]{8}, thread-resume-sp=0x[0-9A-F]{8}, entry=yes, resumed=yes, round-trip=yes")) {
    $missing += "ARMv7-A context switch smoke, main-before=0x..."
}
if (($log -notmatch "ARMv7-A scheduler dispatch, task=svc-trap, isr=timer-tick, task-ready=yes, isr-ready=yes, context-ready=yes, round-trip=yes, dispatch=yes")) {
    $missing += "ARMv7-A scheduler dispatch, task=svc-trap..."
}
if (($log -notmatch "ARMv7-A runtime bridge, tick=yes, isr-defer=yes, yield-svc=0x000043, yield-event=0x00000001, yield-payload=0x00000001, yield-ready=yes, sleep-svc=0x000044, sleep-due=0x0000000000000005, sleep-event=0x00000002, sleep-payload=0x00000005, sleep-ready=yes, dispatch=yes, bridge=yes")) {
    $missing += "ARMv7-A runtime bridge, tick=yes..."
}
if (($log -notmatch "ARMv7-A SGI pending evidence, route=irq, line=group[01]/(yes|no)/(yes|no)/(yes|no), gicd=0x[0-9A-F]{8}, gicc=0x[0-9A-F]{8}, hppir=0x[0-9A-F]{8}, spurious=no")) {
    $missing += "ARMv7-A SGI pending evidence, route=irq..."
}
if (($log -notmatch "ARMv7-A handler stack, vector=irq, mode=irq, sp=0x[0-9A-F]{8}, base=0x[0-9A-F]{8}, top=0x[0-9A-F]{8}, used=0x[0-9A-F]{8}, in-range=yes")) {
    $missing += "ARMv7-A handler stack, vector=irq..."
}
if (($log -notmatch "ARMv7-A FIQ active, intid=1, source=self-sgi, ack=0x[0-9A-F]{8}, hppir-before-ack=0x[0-9A-F]{8}, line=group0/yes/(yes|no)/yes, origin-mode=sys, handler-mode=fiq, return-pc=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A FIQ active, intid=1, source=self-sgi, handler-mode=fiq"
}
if (($log -notmatch "ARMv7-A FIQ complete, intid=1, source=self-sgi, eoi=0x[0-9A-F]{8}, hppir-after-eoi=0x[0-9A-F]{8}, line-after-eoi=group0/yes/(yes|no)/no, active-cleared=yes, controller-advanced=yes, retired=yes")) {
    $missing += "ARMv7-A FIQ complete, intid=1, source=self-sgi"
}
if (($log -notmatch "ARMv7-A FIQ lifecycle, intid=1, source=self-sgi, entry-match=yes, retired=yes, restored=yes, closed=yes")) {
    $missing += "ARMv7-A FIQ lifecycle, intid=1, source=self-sgi"
}
if (($log -notmatch "ARMv7-A SGI pending evidence, route=fiq, line=group[01]/(yes|no)/(yes|no)/(yes|no), gicd=0x[0-9A-F]{8}, gicc=0x[0-9A-F]{8}, hppir=0x[0-9A-F]{8}, spurious=no")) {
    $missing += "ARMv7-A SGI pending evidence, route=fiq..."
}
if (($log -notmatch "ARMv7-A handler stack, vector=fiq, mode=fiq, sp=0x[0-9A-F]{8}, base=0x[0-9A-F]{8}, top=0x[0-9A-F]{8}, used=0x[0-9A-F]{8}, in-range=yes")) {
    $missing += "ARMv7-A handler stack, vector=fiq..."
}
if (($log -notmatch "ARMv7-A return evidence, vector=fiq, origin-mode=sys, current-mode=sys, origin-irq=masked, current-irq=masked, origin-fiq=enabled, current-fiq=enabled, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x[0-9A-F]{8}, base=0x[0-9A-F]{8}, top=0x[0-9A-F]{8}, used=0x[0-9A-F]{8}, in-range=yes")) {
    $missing += "ARMv7-A return evidence, vector=fiq..."
}
if (($log -notmatch "ARMv7-A security side evidence, scr-read=skipped, timer-source=(secure|non-secure)-phys-ppi/group[01], irq-source=self-sgi/group1, irq-origin=[a-z]+, irq-handler=irq, fiq-source=self-sgi/group0, fiq-origin=[a-z]+, fiq-handler=fiq, monitor-mode=(observed|not-observed)")) {
    $missing += "ARMv7-A security side evidence, scr-read=skipped..."
}
if (($log -notmatch "ARMv7-A handoff context, vector-base=0x[0-9A-F]{8}, translation-table=0x[0-9A-F]{8}, image-base=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A handoff context, vector-base=0x..."
}
if (($log -notmatch "ARMv7-A handoff request, kind=(copy|xip), payload-base=0x[0-9A-F]{8}, entry=0x[0-9A-F]{8}, storage-payload=0x[0-9A-F]{8}, storage-entry=0x[0-9A-F]{8}, entry-offset=0x[0-9A-F]{8}, payload-size=0x[0-9A-F]{8}, image-size=0x[0-9A-F]{8}, flags=0x[0-9A-F]{8}")) {
    $missing += "ARMv7-A handoff request, kind=..."
}
if (($log -notmatch "ARMv7-A handoff masked, cpsr=0x[0-9A-F]{8}, irq=masked, fiq=masked")) {
    $missing += "ARMv7-A handoff masked, cpsr=0x..."
}
if (($log -notmatch "ARMv7-A handoff quiesced, cntp_ctl=0x00000002, secure-line=group0/no/no/no, nonsecure-line=group1/no/no/no, sgi-line=group0/yes/no/no, gicd=0x00000000, gicc=0x00000000, hppir=0x000003FF, spurious=yes")) {
    $missing += "ARMv7-A handoff quiesced, cntp_ctl=0x00000002..."
}
if (($log -notmatch "ARMv7-A handoff steps, mask=yes, quiesce=yes, map=yes, dcache=yes, icache=yes, tlb=yes, vectors=yes, sync=yes")) {
    $missing += "ARMv7-A handoff steps, mask=yes..."
}
if (($log -notmatch "ARMv7-A handoff ready, result=yes, vbar=0x[0-9A-F]{8}, ttbr0=0x[0-9A-F]{8}, ttbcr=0x[0-9A-F]{8}, dacr=0x[0-9A-F]{8}, mmu=on, dcache=on, icache=on, irq=masked, fiq=masked")) {
    $missing += "ARMv7-A handoff ready, result=yes..."
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
