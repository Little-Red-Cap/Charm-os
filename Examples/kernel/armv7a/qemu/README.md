# ARMv7-A QEMU bare-metal skeleton

This is the first Cortex-A oriented leaf target for Charm.
It keeps startup, linker, and early UART code inside the example target
instead of pushing ARMv7-A specifics into shared `Modules/`.
The same leaf now also pulls in one explicit minimal kernel/task-syscall
module slice plus the ARMv7-A target-side glue modules, so the lower-half
adapter seam can be proven inside the real bare-metal image without linking
the full `Charm-os`.
The shared ARMv7-A handoff prepare contract now lives in
`targets/armv7a/common/`, carries explicit load/payload/entry metadata,
and lets this QEMU leaf keep the hook implementation and runtime evidence
local. Shared ARMv7-A exception and interrupt contracts also live there, so
frame math, pending/timeout state, observation semantics, and abort-decode
helpers can be validated on the host side before a real board leaf joins the
same path. Fault register/map/context snapshots are now also shaped as a
shared observation contract, while the QEMU leaf still owns the actual CP15
reads and translation-table sampling. PSR decoding plus handler-stack/return
evidence shaping now also live in common contracts, so later Cortex-A boards
can reuse the same observation semantics even when their stack layout hooks
differ.

## Build

```powershell
cmake --preset debug
cmake --build --preset debug --verbose
```

Abort smoke presets stay separate so the default IRQ/SVC smoke remains stable:

```powershell
cmake --preset debug-abort-data
cmake --build out\build\debug-abort-data --verbose

cmake --preset debug-abort-data-align
cmake --build out\build\debug-abort-data-align --verbose

cmake --preset debug-abort-prefetch
cmake --build out\build\debug-abort-prefetch --verbose

cmake --preset debug-abort-prefetch-xn
cmake --build out\build\debug-abort-prefetch-xn --verbose

cmake --preset debug-abort-prefetch-page
cmake --build out\build\debug-abort-prefetch-page --verbose

cmake --preset debug-abort-prefetch-page-xn
cmake --build out\build\debug-abort-prefetch-page-xn --verbose

cmake --preset debug-abort-prefetch-page-xn-runtime
cmake --build out\build\debug-abort-prefetch-page-xn-runtime --verbose

cmake --preset debug-abort-data-perm
cmake --build out\build\debug-abort-data-perm --verbose

cmake --preset debug-abort-data-page
cmake --build out\build\debug-abort-data-page --verbose

cmake --preset debug-abort-data-page-perm
cmake --build out\build\debug-abort-data-page-perm --verbose

cmake --preset debug-abort-data-page-perm-runtime
cmake --build out\build\debug-abort-data-page-perm-runtime --verbose

cmake --preset debug-exception-undefined
cmake --build out\build\debug-exception-undefined --verbose

cmake --preset debug-interrupt-special-irq
cmake --build out\build\debug-interrupt-special-irq --verbose

cmake --preset debug-interrupt-sgi-timeout
cmake --build out\build\debug-interrupt-sgi-timeout --verbose

cmake --preset debug-interrupt-unexpected-irq
cmake --build out\build\debug-interrupt-unexpected-irq --verbose

cmake --preset debug-interrupt-sgi-fiq-timeout
cmake --build out\build\debug-interrupt-sgi-fiq-timeout --verbose

cmake --preset debug-handoff-live
cmake --build out\build\debug-handoff-live --verbose
```

## Run

Build the selected preset first, then launch QEMU:

```powershell
.\run_qemu.ps1
```

Equivalent direct invocation:

```powershell
qemu-system-arm `
  -machine virt -cpu cortex-a7 `
  -nographic -monitor none `
  -device loader,file=out\build\debug\charm-armv7a-qemu,cpu-num=0
```

`-device loader` is used instead of `-kernel` because this target is a
bare-metal ELF, not a Linux kernel image.

Expected console output:

```text
Charm ARMv7-A QEMU skeleton
Targeting Cortex-A7 first, RK3506 later.
ARMv7-A boot state, cpsr=0x600001DF, mode=sys, irq=masked
ARMv7-A reset evidence, sctlr=0x00C50078, vbar=0x00000000, high-vectors=off, low-vectors-forced=no
ARMv7-A cp15 state, sctlr=0x00C50078, vbar=0x40200000, mpidr=0x80000000, cntfrq=0x03B9ACA0
ARMv7-A interrupt reset state, gicd=0x00000000, gicc=0x00000000, pmr=0x00000000, bpr=0x00000000, hppir=0x000003FF, spurious=yes
ARMv7-A timer reset state, cntp_ctl=0x00000000, enabled=no, imask=no, istatus=no, secure-line=group0/no/no/no, nonsecure-line=group0/no/no/no
ARMv7-A SGI reset state, line=group0/yes/no/no
ARMv7-A memory model, id_mmfr0=0x10101105, vmsa=0x00000005 (present), pmsa=0x00000000 (absent)
ARMv7-A feature state, id_pfr1=0x00010001, security=0x00000000 (absent), virtualization=0x00000000 (absent), gentimer=0x00000001 (present)
ARMv7-A cache state, mmu=off, dcache=off, icache=off, high-vectors=off
ARMv7-A translation state, ttbr0=0x00000000, ttbr1=0x00000000, ttbcr=0x00000000, dacr=0x00000000
ARMv7-A L1 table ready, base=0x40210000, ram=0x40211C0E, gic=0x08010C16, uart=0x09010C16
ARMv7-A small-page alias ready, va=0x5200...., pa=0x4020...., l1=0x4020...., l2=0x4020....
ARMv7-A small-page remap ready, va=0x52100000, pa-a=0x4020...., pa-b=0x4020...., l1=0x4021...., l2=0x4021....
ARMv7-A attr probe ready, va=0x52300000, pa=0x40300000, section=0x40300000, identity-l1=0x00000000, l1=0x4021...., l2=0x4030...., tex=0x00000001, mem=normal-cached
ARMv7-A dcache probe ready, va=0x52400000, pa=0x4030...., l1=0x4021...., l2=0x4030....
ARMv7-A icache probe ready, va=0x5220...., pa-a=0x4020...., pa-b=0x4020...., l1=0x4021...., l2=0x4020....
ARMv7-A page-table probe ready, va=0x52500000, pa-a=0x4030...., pa-b=0x4030...., desc=0x4021...., l1=0x4021...., l2=0x4030....
ARMv7-A section-split probe ready, section=0x52600000, addr=0x5260...., pa-section=0x4040...., pa-a=0x4040...., pa-b=0x4040...., l1=0x40400C16
ARMv7-A MMU active, sctlr=0x00C51079, ttbr0=0x40210000, ttbcr=0x00000000, dacr=0x00000001
ARMv7-A MMU flags, mmu=on, dcache=off, icache=on
Charm out.format import active, PL011 @ 0x09000000
ARMv7-A small-page probe, addr=0x5200...., before=0xC0DEF00D, via-alias=0x1BADB002, direct=0x1BADB002
ARMv7-A small-page remap, addr=0x52100000, before=0x13579BDF, after=0x2468ACE0, l2=0x4021....
ARMv7-A attr probe, addr=0x52300000, before=0x11223344, normal=0x55667788, device-before=0x55667788, device=0x99AABBCC, restored=0x99AABBCC
ARMv7-A attr descriptors, normal=0x4030.... (tex=0x00000001, mem=normal-cached), device=0x4030.... (tex=0x00000000, mem=device), restored=0x4030.... (tex=0x00000001, mem=normal-cached)
ARMv7-A icache probe, addr=0x5220...., before=0x000000A1, after=0x000000B2, l2=0x4020....
ARMv7-A D-cache active, sctlr=0x00C5107D, clidr=0x........, ccsidr=0x........, line=0x........, ways=0x........, sets=0x........
ARMv7-A dcache probe, addr=0x52400000, before=0xCAFEBABE, cached=0x10203040, device-before=0x10203040, restored=0x50607080, l2=0x4030....
ARMv7-A page-table probe, addr=0x52500000, before=0x31415926, after=0x27182818, restored=0x31415926, desc=0x4021...., l2=0x4030....
ARMv7-A section-split probe, addr=0x5260...., before=0x89ABCDEF, after=0x76543210, restored=0x89ABCDEF, l1-desc=0x4021...., l2-table=0x4021...., l1=0x4021...., l2=0x4040....
ARMv7-A SVC vector active, imm=0x000043, origin-mode=sys, handler-mode=svc, return-pc=0x4020....
ARMv7-A handler stack, vector=svc, mode=svc, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A SVC vector active, imm=0x000044, origin-mode=sys, handler-mode=svc, return-pc=0x4020....
ARMv7-A handler stack, vector=svc, mode=svc, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A return evidence, vector=svc, origin-mode=sys, current-mode=sys, origin-irq=masked, current-irq=masked, origin-fiq=masked, current-fiq=masked, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A timer pending evidence, cntp_ctl=0x00000001, secure-line=group0/yes/no/no, nonsecure-line=group1/yes/yes/no, gicd=0x00000003, gicc=0x00000007, hppir=0x0000001E, spurious=no
ARMv7-A timer IRQ active, intid=30, origin-mode=sys, handler-mode=irq, return-pc=0x4020....
ARMv7-A handler stack, vector=irq, mode=irq, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A return evidence, vector=irq, origin-mode=sys, current-mode=sys, origin-irq=enabled, current-irq=enabled, origin-fiq=masked, current-fiq=masked, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A SGI pending evidence, route=irq, line=group1/yes/yes/no, gicd=0x00000003, gicc=0x00000007, hppir=0x00000001, spurious=no
ARMv7-A SGI active, intid=1, origin-mode=sys, handler-mode=irq, return-pc=0x4020....
ARMv7-A handler stack, vector=irq, mode=irq, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A return evidence, vector=irq, origin-mode=sys, current-mode=sys, origin-irq=enabled, current-irq=enabled, origin-fiq=masked, current-fiq=masked, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A SGI pending evidence, route=fiq, line=group0/yes/yes/no, gicd=0x00000003, gicc=0x0000000F, hppir=0x00000001, spurious=no
ARMv7-A FIQ active, intid=1, origin-mode=sys, handler-mode=fiq, return-pc=0x4020....
ARMv7-A handler stack, vector=fiq, mode=fiq, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A return evidence, vector=fiq, origin-mode=sys, current-mode=sys, origin-irq=masked, current-irq=masked, origin-fiq=enabled, current-fiq=enabled, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x4050...., base=0x4050...., top=0x4050...., used=0x000000.., in-range=yes
ARMv7-A security side evidence, scr-read=skipped, timer-route=non-secure-phys-ppi, irq-origin=sys, irq-handler=irq, fiq-origin=sys, fiq-handler=fiq, monitor-mode=not-observed
ARMv7-A kernel ingress, vector-base=0x40200000, tick-mode=oneshot, tick-route=irq, timer-hz=62500000, exception=yes, interrupt=yes, timer=yes, context-ready=yes, context-model=software-frame, tick-runtime=yes, thread-runtime=yes
ARMv7-A scheduler tick ingress, source=timer-irq, route=irq, mode=oneshot, intid=30, hz=62500000, now=0x00000000........, source-match=yes, counter=yes, isr-safe=yes, retired=yes, handoff=yes, rearm=yes
ARMv7-A runtime trap frame, yield-path=svc-frame, yield-handler=svc, yield-return-pc=0x4020...., yield-ready=yes, sleep-path=svc-frame, sleep-handler=svc, sleep-return-pc=0x4020...., sleep-ready=yes, frame=yes
ARMv7-A runtime trap ingress, source=svc, service=0x000043, arg0=0x00000001, arg1=0x00000001, arg2=0x00000000, arg3=0x00000000, service-ready=yes, args-ready=yes, trap=yes
ARMv7-A runtime trap mapping, yield=yield-current, yield-generic=0x0001, yield-origin=kernel-thread, yield-return-pc=0x4020...., yield-ready=yes, sleep=sleep-until, sleep-generic=0x0002, sleep-origin=kernel-thread, sleep-due=0x0000000000000005, sleep-ready=yes, mapping=yes
ARMv7-A runtime trap adapter, yield-path=svc-r0, yield-r0=0x00000001, yield-preserve=yes, yield-ready=yes, sleep-path=svc-r0, sleep-r0=0x00000005, sleep-preserve=yes, sleep-ready=yes, adapter=yes
ARMv7-A runtime trap seam, yield-path=svc-frame-r0, yield-generic=0x0001, yield-origin=kernel-thread, yield-r0=0x00000001, yield-ready=yes, sleep-path=svc-frame-r0, sleep-generic=0x0002, sleep-origin=kernel-thread, sleep-r0=0x00000005, sleep-ready=yes, seam=yes
ARMv7-A runtime trap live-adapter, yield-path=svc-live-frame, yield-generic=0x0001, yield-r0=0x00000001, yield-ready=yes, sleep-path=svc-live-frame, sleep-generic=0x0002, sleep-r0=0x00000005, sleep-ready=yes, live-adapter=yes
ARMv7-A runtime trap ingress-adapter, yield-path=live-frame-adapter, yield-generic=0x0001, yield-r0=0x00000001, yield-ready=yes, sleep-path=live-frame-adapter, sleep-generic=0x0002, sleep-r0=0x00000005, sleep-ready=yes, ingress-adapter=yes
ARMv7-A runtime trap caller, yield-path=svc-call-frame, yield-svc=0x000043, yield-r0=0x00000001, yield-ready=yes, sleep-path=svc-call-frame, sleep-svc=0x000044, sleep-due=0x0000000000000005, sleep-r0=0x00000005, sleep-ready=yes, caller=yes
ARMv7-A runtime trap failure, unsupported=unsupported-service, decode=decode-failed, writeback=writeback-failed, adapter=unbound-adapter, dispatch=unbound-adapter, failure=yes
ARMv7-A thread frame, kind=cooperative-sys, stack-base=0x40...., stack-top=0x40...., prepared-sp=0x40...., resume=0x40...., return=0x40...., entry=0x40...., arg=0x40...., aligned=yes, in-range=yes, ready=yes
ARMv7-A context switch smoke, main-before=0x40...., main-saved=0x40...., thread-entry-sp=0x40...., thread-saved=0x40...., thread-resume-sp=0x40...., entry=yes, resumed=yes, round-trip=yes
ARMv7-A thread runtime, kind=cooperative-sys, task=0x0000000059537001, current-sp=0x000000005200B000, prepared-sp=0x40...., current=yes, prepare=yes, switch=yes, runtime=yes
ARMv7-A scheduler dispatch, task=svc-trap, isr=timer-tick, task-ready=yes, isr-ready=yes, context-ready=yes, round-trip=yes, current=yes, dispatch=yes
ARMv7-A runtime bridge, tick=yes, isr-defer=yes, yield-svc=0x000043, yield-event=0x00000001, yield-payload=0x00000001, yield-ready=yes, sleep-svc=0x000044, sleep-due=0x0000000000000005, sleep-event=0x00000001, sleep-payload=0x00000005, sleep-ready=yes, dispatch=yes, bridge=yes
ARMv7-A runtime loop ingress, mode=oneshot, route=irq, hz=62500000, tick-runtime=yes, thread=yes, tick=yes, isr-defer=yes, idle=yes, worker=yes, run=yes, loop=yes
ARMv7-A runtime leaf ports, tick-mode=oneshot, tick-route=irq, exception=yes, interrupt=yes, timer=yes, context=yes, current=yes, hook=yes, loop=yes, trap=yes, call=yes, thread=yes, shared=yes, ports=yes
ARMv7-A runtime thread port, yield-path=svc-call-frame, yield-ready=yes, sleep-path=svc-call-frame, sleep-ready=yes, trap-call=yes, thread-runtime=yes, port=yes, bridge=yes, thread=yes
ARMv7-A runtime live, task=yes, trap=yes, timer=yes, tick=yes, idle=yes, worker=yes, live=yes, resumes=3, idle-runs=1, wake-due=0x000000000000...., tick-now=0x000000000000....
ARMv7-A runtime binding bundle, current=yes, trap=yes, thread=yes, loop=yes, live=yes, export=yes, binding=yes
ARMv7-A runtime leaf bundle, tick-mode=oneshot, tick-route=irq, exception=yes, interrupt=yes, timer=yes, context=yes, current=yes, hook=yes, loop=yes, trap=yes, call=yes, thread=yes, live=yes, export=yes, ports=yes, bundle=yes
ARMv7-A runtime package, leaf=yes, binding=yes, current=yes, trap=yes, call=yes, thread=yes, loop=yes, live=yes, derived=yes, export=yes, package=yes
ARMv7-A task syscall frame, debug-path=svc-frame, debug-svc=0x000045, debug-generic=0x0003, debug-task=0x0000000059532001, debug-ready=yes, capability-path=svc-frame, capability-svc=0x000046, capability-generic=0x0004, capability-task=0x0000000059532001, capability-ready=yes, frame=yes
ARMv7-A task syscall dispatch, debug-path=dispatch-port, debug-generic=0x0003, debug-task=0x0000000059533001, debug-r0=0x00000044, debug-ready=yes, capability-path=dispatch-port, capability-generic=0x0004, capability-task=0x0000000059533001, capability-r0=0x0000002A, capability-ready=yes, dispatch=yes
ARMv7-A task syscall surface, debug-path=live-svc-dispatch, debug-svc=0x000045, debug-generic=0x0003, debug-r0=0x00000044, debug-ready=yes, capability-path=live-svc-dispatch, capability-svc=0x000046, capability-generic=0x0004, capability-r0=0x0000002A, capability-ready=yes, surface=yes
ARMv7-A task syscall ingress-adapter, debug-path=live-frame-adapter, debug-generic=0x0003, debug-r0=0x00000044, debug-ready=yes, capability-path=live-frame-adapter, capability-generic=0x0004, capability-r0=0x0000002A, capability-ready=yes, ingress-adapter=yes
ARMv7-A task syscall caller, debug-path=svc-call-frame, debug-svc=0x000045, debug-generic=0x0003, debug-r0=0x00000044, debug-ready=yes, capability-path=svc-call-frame, capability-svc=0x000046, capability-generic=0x0004, capability-r0=0x0000002A, capability-ready=yes, caller=yes
ARMv7-A task syscall roundtrip, debug-path=svc-return, debug-svc=0x000045, debug-value=0x00000044, debug-ready=yes, capability-path=svc-return, capability-svc=0x000046, capability-value=0x0000002A, capability-ready=yes, roundtrip=yes
ARMv7-A task syscall glue, task=0x0000000059534001, stack=0x0000000052008000, yield=0x00000001, sleep=0x00000037, debug=0x000000CD, capability=0x0000002A, generic=yes, ingress=yes, bridge=yes, caller=yes, api=yes, glue=yes
ARMv7-A task syscall failure, decode=decode-failed, unsupported=unsupported-service, bridge=unbound-bridge, caller=unbound-adapter, writeback=writeback-failed, failure=yes
ARMv7-A handoff context, vector-base=0x40200000, translation-table=0x4021...., image-base=0x40200000
ARMv7-A handoff request, kind=copy, payload-base=0x40200000, entry=0x40200000, storage-payload=0x00000000, storage-entry=0x00000000, entry-offset=0x00000000, payload-size=0x00000000, image-size=0x00000000, flags=0x00000000
ARMv7-A handoff masked, cpsr=0x........, irq=masked, fiq=masked
ARMv7-A handoff quiesced, cntp_ctl=0x00000002, secure-line=group0/no/no/no, nonsecure-line=group1/no/no/no, sgi-line=group0/yes/no/no, gicd=0x00000000, gicc=0x00000000, hppir=0x000003FF, spurious=yes
ARMv7-A handoff steps, mask=yes, quiesce=yes, map=yes, dcache=yes, icache=yes, tlb=yes, vectors=yes, sync=yes
ARMv7-A runtime handoff, runtime=yes, context=yes, hooks=yes, vector=yes, report=yes, export=yes, handoff=yes
ARMv7-A handoff entry, target=0x40200000, request=yes, offset=yes, mode=sys, vector=yes, translation=yes, cache=yes, masks=yes, export=yes, entry=yes
ARMv7-A handoff ready, result=yes, vbar=0x40200000, ttbr0=0x4021...., ttbcr=0x00000000, dacr=0x00000001, mmu=on, dcache=on, icache=on, irq=masked, fiq=masked
ARMv7-A handoff transfer, target=0x40200000, arg0=0x4023...., size=0x00000138, mode=sys, state=arm, entry=yes, payload=yes, stack=yes, export=yes, transfer=yes
ARMv7-A handoff launch, target=0x40200000, arg0=0x4023...., mode=sys, state=arm, transfer=yes, hook=yes, route=yes, probe=yes, arg0=yes, next=yes, stack=yes, branch=yes, link=yes, return=yes, invoke=yes, launch=yes
```

## CI smoke

```powershell
.\run_qemu_ci.ps1
```

`run_qemu_ci.ps1` now configures and clean-rebuilds the default `debug`
preset before launching QEMU, so the smoke log stays aligned with the current
source instead of whatever ELF or stale object graph happened to be left in
`out\build\debug`. The default build leg now also uses `--parallel 1`, which
keeps the ARM bare-metal GCC modules output stable during CI smoke runs. The
same smoke now also gates the
`runtime-leaf-ports`, `runtime-live`, `runtime-binding-bundle`, and
`runtime-leaf-bundle` phase markers, plus the
`ARMv7-A runtime leaf ports, ... call=yes, thread=yes, ... ports=yes`,
the `ARMv7-A runtime thread port, ... thread=yes` summary,
the `ARMv7-A runtime binding bundle, ... export=yes, binding=yes` summary,
`ARMv7-A runtime live, ... live=yes`, and
the `ARMv7-A runtime leaf bundle, ... export=yes, ... bundle=yes` summary,
the `ARMv7-A runtime package, leaf=yes, binding=yes, ... package=yes` summary,
the `ARMv7-A runtime handoff, runtime=yes, ... handoff=yes` summary, and
the `ARMv7-A handoff entry, target=0x..., ... entry=yes` summary, and
the `ARMv7-A handoff transfer, target=0x..., ... transfer=yes` summary, and
the `ARMv7-A handoff launch, target=0x..., ... launch=yes` summary, and
treats `ARMv7-A runtime live debug` as unexpected output so the mainline log
only stays green once the live path is fully closed.

For a phase-ledger view of the same default boot, use:

```powershell
.\run_qemu_phase_ledger_ci.ps1
```

That focused smoke treats the serial log as an ordered stage ledger. It checks
that the default QEMU image moves from boot evidence through runtime,
task-syscall, and handoff launch in order, that every non-idle stage has a
matching `phase complete` marker, and that optional edge-only phases such as
`special-irq-smoke` or `handoff-live` do not leak into the default boot.

For a shorter failure loop around just this lower-half seam, use the focused
runtime-live smoke:

```powershell
.\run_qemu_runtime_live_ci.ps1
```

For a shorter failure loop around the exported live-session leaf ports, use:

```powershell
.\run_qemu_runtime_leaf_ports_ci.ps1
```

For a shorter failure loop around the exported runtime-thread egress seam, use:

```powershell
.\run_qemu_runtime_thread_ci.ps1
```

For a dedicated arch ingress seam closure smoke around exception, timer,
context, runtime-trap, and runtime-loop ingress, use:

```powershell
.\run_qemu_arch_ingress_seam_ci.ps1
```

That smoke stays on the shared `debug` preset and only checks that the lower
half still exposes one coherent ingress seam; it does not add new OS, user
mode, or compare behavior.

For a dedicated top-level witness re-export that lifts the real runtime bundle
into the shared `system_compiler.witness_bundle/v0` surface, use:

```powershell
..\..\..\..\scripts\minimal_kernel_runtime_system_compiler_witness_bundle.ps1 `
  -OutputRoot out\minimal-kernel-runtime-system-compiler-witness `
  -HostExamples runtime_minimal_host `
  -HostJobs 1 `
  -QemuBuildJobs 1
```

That wrapper keeps the existing host and QEMU bundle flow intact, then
re-exports the result through
`Examples/kernel/canonical_worlds/minimal_kernel_runtime.world.json` and
validates the final witness bundle references with
`scripts/validate_system_compiler_witness_bundle.py`. When the intermediate
runtime bundle already supports `-SkipWitnessBundle`, this wrapper also asks
it to suppress the nested witness export so this command stays the single
top-level witness outlet.

For a dedicated self-compare smoke that proves the current runtime world still
stands against itself, use:

```powershell
..\..\..\..\scripts\ci_minimal_kernel_runtime_system_compiler_world_compare.ps1 `
  -OutputRoot out\minimal-kernel-runtime-system-compiler-world-compare `
  -HostExamples runtime_minimal_host `
  -HostJobs 1 `
  -QemuBuildJobs 1
```

That entry runs the same top-level witness export, but if no explicit baseline
is provided it reuses the current runtime-evidence path as both baseline and
candidate, then asks `world_compare/summary.json` to stay at
`world_verdict=standing`.

For a dedicated real non-returning handoff landing smoke, use:

```powershell
.\run_qemu_handoff_live_ci.ps1
```

That live handoff preset keeps the default returnable `handoff launch` probe
untouched in the main `debug` smoke, but swaps in one separate synthetic
next-stage landing path for the dedicated smoke build. The live path only goes
green once QEMU really branches through the explicit launch route, lands in the
synthetic next-stage entry with the inherited `r0/sp` shape, completes the
`handoff-launch` phase from the landing side, reports one
`ARMv7-A handoff live, ... live=yes` line, then proves the handed-over runtime
package can be re-armed in place and directly re-consumed from the landing
side via one `ARMv7-A runtime handoff landing, ... rearm=yes, payload=yes,
... landed=yes` line, one `ARMv7-A runtime handoff leaf landing, ...
leaf=yes` line, one `ARMv7-A runtime handoff binding landing, ...
binding=yes` line, one `ARMv7-A runtime handoff session landing, ...
session=yes` line, one `ARMv7-A runtime handoff package landing, ...
landing=yes` line, one `ARMv7-A runtime handoff landing bundle, ...
bundle=yes` line, one `ARMv7-A runtime handoff consumer, ... consumer=yes`
line, plus one `ARMv7-A runtime handoff path, ... path=yes` line before
idling forever.

For a shorter failure loop around the runtime-facing binding bundle, use:

```powershell
.\run_qemu_runtime_binding_bundle_ci.ps1
```

For a shorter failure loop around the board-facing runtime package, use:

```powershell
.\run_qemu_runtime_package_ci.ps1
```

For a shorter failure loop around the board-facing handoff seam, use:

```powershell
.\run_qemu_handoff_ci.ps1
```

For a shorter failure loop around the exported lower-half leaf payload, use:

```powershell
.\run_qemu_runtime_leaf_bundle_ci.ps1
```

For a shorter failure loop around the runtime-trap ingress/capture/writeback
seam, use:

```powershell
.\run_qemu_runtime_trap_ci.ps1
```

For a shorter failure loop around the live task-syscall seam, use:

```powershell
.\run_qemu_task_syscall_ci.ps1
```

To run the lower-half focused bundle with one clean shared `debug` rebuild,
use:

```powershell
.\run_qemu_lower_half_ci.ps1
```

That aggregate lower-half smoke now clean-rebuilds the shared `debug` preset
before fanning out into the phase-ledger, focused runtime/task-syscall cases,
and also asks the dedicated `handoff-live` smoke to clean-rebuild
`debug-handoff-live`.
This keeps wide lower-half regressions from being masked or invented by stale
object files after contract/header layout changes.
The same bundle output now also carries one explicit `canonical world`,
`witness bundle`, and short runtime `biography`, so the lower-half artifact can
say what world it represents, what question it is answering, and which witness
cases currently keep that world standing.

Abort smoke CI is intentionally separate because these runs end in the fatal
exception path instead of returning to the regular SVC/IRQ smoke:

```powershell
.\run_qemu_abort_ci.ps1 -Kind data
.\run_qemu_abort_ci.ps1 -Kind data-align
.\run_qemu_abort_ci.ps1 -Kind prefetch
.\run_qemu_abort_ci.ps1 -Kind prefetch-xn
.\run_qemu_abort_ci.ps1 -Kind prefetch-page
.\run_qemu_abort_ci.ps1 -Kind prefetch-page-xn
.\run_qemu_abort_ci.ps1 -Kind prefetch-page-xn-runtime
.\run_qemu_abort_ci.ps1 -Kind data-perm
.\run_qemu_abort_ci.ps1 -Kind data-page
.\run_qemu_abort_ci.ps1 -Kind data-page-perm
.\run_qemu_abort_ci.ps1 -Kind data-page-perm-runtime
```

Undefined exception smoke also has a dedicated CI entry because it ends in the
fatal undefined handler instead of continuing into the later IRQ/FIQ smoke:

```powershell
.\run_qemu_exception_ci.ps1 -Kind undefined
```

The synthetic special-ack edge smoke is also kept separate so the default
success-path CI stays stable while we harden the failure-path evidence:

```powershell
.\run_qemu_interrupt_special_ci.ps1
```

```powershell
.\run_qemu_interrupt_sgi_timeout_ci.ps1
```

```powershell
.\run_qemu_interrupt_unexpected_ci.ps1
```

```powershell
.\run_qemu_interrupt_sgi_fiq_timeout_ci.ps1
```

## Special IRQ acknowledge smoke

Use the dedicated preset and pass its ELF to `run_qemu.ps1`:

```powershell
.\run_qemu.ps1 -ElfPath out\build\debug-interrupt-special-irq\charm-armv7a-qemu
```

Expected output includes the same normal bringup banner as the default smoke,
then adds one synthetic IRQ acknowledge probe before idle:

```text
ARMv7-A phase, stage=special-irq-smoke
ARMv7-A diagnostic context, subsystem=interrupt, stage=special-irq-smoke, last-complete=sgi-fiq-smoke, cpsr=0x........
ARMv7-A special IRQ acknowledge, intid=1023, source=special-intid, ack=0x000003FF, hppir-before-ack=0x000003FF, route=irq, origin-mode=sys, current-mode=sys, return-pc=0x40000000, synthetic=yes
ARMv7-A phase complete, stage=special-irq-smoke
```

## SGI IRQ timeout smoke

Use the dedicated preset and pass its ELF to `run_qemu.ps1`:

```powershell
.\run_qemu.ps1 -ElfPath out\build\debug-interrupt-sgi-timeout\charm-armv7a-qemu
```

Expected output includes the normal bringup banner, one SGI pending snapshot,
then a timeout summary captured before the smoke cleans up the GIC state:

```text
ARMv7-A phase, stage=sgi-irq-timeout-smoke
ARMv7-A SGI pending evidence, route=irq, line=group1/yes/yes/no, gicd=0x00000003, gicc=0x00000007, hppir=0x00000001, spurious=no
ARMv7-A diagnostic context, subsystem=interrupt, stage=sgi-irq-timeout-smoke, last-complete=sgi-fiq-smoke, cpsr=0x........
ARMv7-A interrupt timeout, expected=sgi-irq, route=irq, route-mask=masked, pending-observed=yes, last-observation=not-observed
ARMv7-A SGI timeout, igroupr0=0x00000000, isenabler0=0x00000002, ispendr0=0x00000002, isactiver0=0x00000000, hppir=0x00000001
ARMv7-A phase complete, stage=sgi-irq-timeout-smoke
```

## Unexpected IRQ smoke

Use the dedicated preset and pass its ELF to `run_qemu.ps1`:

```powershell
.\run_qemu.ps1 -ElfPath out\build\debug-interrupt-unexpected-irq\charm-armv7a-qemu
```

Expected output includes the normal bringup banner, one pending snapshot for
`SGI intid=2`, then the handler-side unexpected-intid contract:

```text
ARMv7-A phase, stage=unexpected-irq-smoke
ARMv7-A unexpected IRQ pending evidence, intid=0x00000002, source=unexpected-intid, route=irq, line=group1/yes/yes/no, gicd=0x00000003, gicc=0x00000007, hppir=0x00000002, spurious=no
ARMv7-A diagnostic context, subsystem=interrupt, stage=unexpected-irq-smoke, last-complete=sgi-fiq-smoke, cpsr=0x........
ARMv7-A unexpected IRQ, intid=0x00000002, source=unexpected-intid, ack=0x00000002, hppir-before-ack=0x00000002, line=group1/yes/(yes|no)/yes, origin-mode=sys, handler-mode=irq, return-pc=0x........, pc=0x........, lr=0x........, spsr=0x........
ARMv7-A return evidence, vector=irq, origin-mode=sys, current-mode=sys, origin-irq=enabled, current-irq=enabled, origin-fiq=masked, current-fiq=masked, mode-restored=yes, irq-restored=yes, fiq-restored=yes, sp=0x........, base=0x........, top=0x........, used=0x........, in-range=yes
ARMv7-A phase complete, stage=unexpected-irq-smoke
```

## SGI FIQ timeout smoke

Use the dedicated preset and pass its ELF to `run_qemu.ps1`:

```powershell
.\run_qemu.ps1 -ElfPath out\build\debug-interrupt-sgi-fiq-timeout\charm-armv7a-qemu
```

Expected output includes the normal bringup banner, one Group0 SGI pending
snapshot, then a timeout summary captured while CPU FIQ is still masked:

```text
ARMv7-A phase, stage=sgi-fiq-timeout-smoke
ARMv7-A SGI pending evidence, route=fiq, line=group0/yes/yes/no, gicd=0x00000003, gicc=0x0000000F, hppir=0x00000001, spurious=no
ARMv7-A diagnostic context, subsystem=interrupt, stage=sgi-fiq-timeout-smoke, last-complete=sgi-fiq-smoke, cpsr=0x........
ARMv7-A interrupt timeout, expected=sgi-fiq, route=fiq, route-mask=masked, pending-observed=yes, last-observation=not-observed
ARMv7-A FIQ timeout, cpsr=0x........, ctlr=0x0000000F, igroupr0=0x00000000, isenabler0=0x00000002, hppir=0x00000001
ARMv7-A phase complete, stage=sgi-fiq-timeout-smoke
```

## Undefined exception smoke

Use the dedicated preset and pass its ELF to `run_qemu.ps1`:

```powershell
.\run_qemu.ps1 -ElfPath out\build\debug-exception-undefined\charm-armv7a-qemu
```

Expected output includes the same boot/MMU banner as the default smoke, then
stops in the undefined handler before the later D-cache and IRQ smoke:

```text
ARMv7-A exception smoke, kind=undefined
ARMv7-A exception: undefined, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=und
ARMv7-A handler stack, vector=undefined, mode=und, sp=0x........, base=0x........, top=0x........, used=0x........, in-range=yes
```

## Abort smoke

Use the dedicated preset and pass its ELF to `run_qemu.ps1`:

```powershell
.\run_qemu.ps1 -ElfPath out\build\debug-abort-data\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-data-align\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-prefetch\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-prefetch-xn\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-prefetch-page\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-prefetch-page-xn\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-prefetch-page-xn-runtime\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-data-perm\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-data-page\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-data-page-perm\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-data-page-perm-runtime\charm-armv7a-qemu
```

Expected abort-mode output includes the same boot/MMU banner as the default
smoke, then stops in one of these fault paths instead of printing the later
SVC/IRQ lines. The fatal exception header is now also followed by one
`ARMv7-A handler stack, vector=...` line proving the abort handler ran on the
expected `abt` banked stack:

```text
ARMv7-A abort smoke, kind=data, addr=0x20000000
ARMv7-A exception: data abort, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A data fault, dfsr=0x........, dfar=0x20000000, adfsr=0x........
ARMv7-A data fault decode, status=0x05 (section translation fault), domain=0x0, write=no, cm=no
ARMv7-A fault map, far=0x20000000, ttbr0=0x........, l1[0x200]=0x00000000 (fault)
```

```text
ARMv7-A data-align target ready, addr=0x4020...., base=0x4020...., value=0x89ABCDEF
ARMv7-A alignment trap armed, addr=0x4020...., base=0x4020...., sctlr=0x00C5107B, alignment-check=on
ARMv7-A abort smoke, kind=data-align, addr=0x4020....
ARMv7-A exception: data abort, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A data fault, dfsr=0x00000001, dfar=0x4020...., adfsr=0x00000000
ARMv7-A data fault decode, status=0x01 (alignment exception), domain=0x0, write=no, cm=no
ARMv7-A fault map, far=0x4020...., ttbr0=0x........, l1[0x402]=0x........ (section), domain=0x0, xn=no, s=yes, c=yes, b=yes, ap=0x3
```

```text
ARMv7-A abort smoke, kind=prefetch, addr=0x20000000
ARMv7-A exception: prefetch abort, pc=0x20000000, lr=0x20000004, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A prefetch fault, ifsr=0x........, ifar=0x20000000, aifsr=0x........
ARMv7-A prefetch fault decode, status=0x05 (section translation fault), domain=0x0
ARMv7-A fault map, far=0x20000000, ttbr0=0x........, l1[0x200]=0x00000000 (fault)
```

```text
ARMv7-A XN alias ready, va=0x5......., pa=0x4......., desc=0x........
ARMv7-A abort smoke, kind=prefetch-xn, addr=0x5.......
ARMv7-A exception: prefetch abort, pc=0x5......., lr=0x5......., spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A prefetch fault, ifsr=0x0000001D, ifar=0x5......., aifsr=0x........
ARMv7-A prefetch fault decode, status=0x0D (section permission fault), domain=0x1
ARMv7-A fault map, far=0x5......., ttbr0=0x........, l1[0x500]=0x........ (section), domain=0x1, xn=yes, s=yes, c=yes, b=yes, ap=0x3
```

```text
ARMv7-A prefetch-page alias ready, va=0x56......, l1=0x........, l2=0x00000000
ARMv7-A abort smoke, kind=prefetch-page, addr=0x56......
ARMv7-A exception: prefetch abort, pc=0x56......, lr=0x56......, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A prefetch fault, ifsr=0x00000007, ifar=0x56......, aifsr=0x........
ARMv7-A prefetch fault decode, status=0x07 (page translation fault), domain=0x0
ARMv7-A fault map, far=0x56......, ttbr0=0x........, l1[0x560]=0x........ (page table), domain=0x0, l2[0x00]=0x00000000 (fault)
```

```text
ARMv7-A page-XN alias ready, va=0x55......, pa=0x4......., l1=0x........, l2=0x........
ARMv7-A abort smoke, kind=prefetch-page-xn, addr=0x55......
ARMv7-A exception: prefetch abort, pc=0x55......, lr=0x55......, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A prefetch fault, ifsr=0x0000001F, ifar=0x55......, aifsr=0x........
ARMv7-A prefetch fault decode, status=0x0F (page permission fault), domain=0x1
ARMv7-A fault map, far=0x55......, ttbr0=0x........, l1[0x550]=0x........ (page table), domain=0x1, l2[0x00]=0x........ (small page), xn=yes, s=yes, c=yes, b=yes, ap=0x3
```

```text
ARMv7-A runtime page-XN alias ready, va=0x58......, pa=0x4......., l1=0x........, l2=0x........
ARMv7-A runtime page-XN probe, addr=0x58......, return=0x00000043
ARMv7-A runtime page-XN flip, addr=0x58......, l2=0x........
ARMv7-A abort smoke, kind=prefetch-page-xn-runtime, addr=0x58......
ARMv7-A exception: prefetch abort, pc=0x58......, lr=0x58......, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A prefetch fault, ifsr=0x0000001F, ifar=0x58......, aifsr=0x........
ARMv7-A prefetch fault decode, status=0x0F (page permission fault), domain=0x1
ARMv7-A fault map, far=0x58......, ttbr0=0x........, l1[0x580]=0x........ (page table), domain=0x1, l2[0x00]=0x........ (small page), xn=yes, s=yes, c=yes, b=yes, ap=0x3
```

```text
ARMv7-A data alias ready, va=0x5......., pa=0x4......., desc=0x........
ARMv7-A abort smoke, kind=data-perm, addr=0x5......., value=0xA5A55A5A
ARMv7-A exception: data abort, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A data fault, dfsr=0x0000081D, dfar=0x5......., adfsr=0x........
ARMv7-A data fault decode, status=0x0D (section permission fault), domain=0x1, write=yes, cm=no
ARMv7-A fault map, far=0x5......., ttbr0=0x........, l1[0x510]=0x........ (section), domain=0x1, xn=yes, s=yes, c=yes, b=yes, ap=0x0
```

```text
ARMv7-A data-page alias ready, va=0x53000040, l1=0x........, l2=0x00000000
ARMv7-A abort smoke, kind=data-page, addr=0x53000040
ARMv7-A exception: data abort, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A data fault, dfsr=0x00000007, dfar=0x53000040, adfsr=0x........
ARMv7-A data fault decode, status=0x07 (page translation fault), domain=0x0, write=no, cm=no
ARMv7-A fault map, far=0x53000040, ttbr0=0x........, l1[0x530]=0x........ (page table), domain=0x0, l2[0x00]=0x00000000 (fault)
```

```text
ARMv7-A data-page-perm alias ready, va=0x54......, pa=0x4......., l1=0x........, l2=0x........
ARMv7-A abort smoke, kind=data-page-perm, addr=0x54......, value=0xA5A55A5A
ARMv7-A exception: data abort, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A data fault, dfsr=0x0000081F, dfar=0x54......, adfsr=0x........
ARMv7-A data fault decode, status=0x0F (page permission fault), domain=0x1, write=yes, cm=no
ARMv7-A fault map, far=0x54......, ttbr0=0x........, l1[0x540]=0x........ (page table), domain=0x1, l2[0x00]=0x........ (small page), xn=yes, s=yes, c=yes, b=yes, ap=0x0
```

```text
ARMv7-A runtime data-page alias ready, va=0x57......, pa=0x4......., l1=0x........, l2=0x........
ARMv7-A runtime data-page probe, addr=0x57......, before=0x0BADCAFE, after=0x5AA55AA5, direct=0x5AA55AA5
ARMv7-A runtime data-page flip, addr=0x57......, l2=0x........
ARMv7-A abort smoke, kind=data-page-perm-runtime, addr=0x57......, value=0xA5A55A5A
ARMv7-A exception: data abort, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A data fault, dfsr=0x0000081F, dfar=0x57......, adfsr=0x........
ARMv7-A data fault decode, status=0x0F (page permission fault), domain=0x1, write=yes, cm=no
ARMv7-A fault map, far=0x57......, ttbr0=0x........, l1[0x570]=0x........ (page table), domain=0x1, l2[0x00]=0x........ (small page), xn=yes, s=yes, c=yes, b=yes, ap=0x0
```

## GDB attach

Start QEMU and wait for a debugger:

```powershell
.\run_qemu.ps1 -WaitForGdb
```

Then in another terminal:

```text
arm-none-eabi-gdb out\build\debug\charm-armv7a-qemu
target remote :1234
break main
continue
```

## Notes

- Platform-facing console, debug trace, and idle hooks are routed through
  `armv7a_platform_*` declarations in `armv7a_platform.hpp`.
- Reset-time platform hooks now also route early reset sequencing and
  exception-vector installation through `armv7a_platform_*`, so `startup.S`
  no longer writes `VBAR` or owns the terminal idle loop directly.
- The current QEMU `virt` reset hook also forces `SCTLR.V=0` before installing
  `VBAR`, so the example does not quietly depend on the reset state already
  using low vectors.
- That same reset hook now also records the initial `SCTLR/VBAR` evidence
  printed by the boot banner, which gives later real-board bring-up a direct
  comparison point against the QEMU reset state.
- Timer and interrupt controller setup also flow through
  `armv7a_platform_*`, so the smoke paths no longer need direct GIC or Generic
  Timer knowledge.
- `early_uart.cpp` and `qemu_virt_platform.cpp` provide the current QEMU `virt`
  implementation so upper layers no longer need to reference PL011 or semihost
  details directly.
- `qemu_virt_platform_interrupts.cpp` provides the current QEMU `virt`
  implementation for timer and interrupt routing while preserving a platform
  contract for future boards.
- The linker starts at `0x40200000` to stay clear of the `virt` DTB area
  near `0x40000000`.
- Current scope is intentionally small: reset entry, per-mode stacks,
  VBAR/vector setup, one returning SVC smoke, one returning timer IRQ
  smoke, one returning self-targeted SGI IRQ smoke, one returning
  self-targeted FIQ smoke, and early PL011 UART on QEMU `virt`.
- ARMv7-A specific inline assembly is now funneled into dedicated leaf-target
  helpers such as `armv7a_cpu.cpp` and `armv7a_arch_timer.cpp`, so the
  higher-level smoke tests stay mostly plain C++.
- SVC, IRQ, and fatal vectors now build one shared exception frame shape
  before entering C++, which gives later abort/MMU bring-up work a more
  stable place to inspect CPU state.
- Translation and fault-status CP15 registers are now wrapped in
  `armv7a_mmu.cpp`, so we can inspect MMU/cache state today and reuse the
  same accessors when we start building page tables later.
- A first 16KB short-descriptor L1 identity map is now prepared in RAM, but
  is now also wired into `TTBR0` with `TTBCR=0`, `DACR=0x1`, and a first
  `SCTLR.M` enable step. The default runtime now stays in `domain0 client`
  instead of leaning on `domain0 manager`, then enables `SCTLR.I` right after
  MMU bring-up and only enables `SCTLR.C` later in the default smoke after the
  earlier mapping/permission probes have already run, so the first D-cache
  step does not get mixed into the same debug window as the very first MMU
  enable.
- The timer smoke prepares both architected physical timer PPIs.
  Current QEMU `virt` runs observed the non-secure physical timer route
  (`intid=30`), but the code also accepts the secure route (`intid=29`)
  so the same leaf target is less brittle across reset states.
- A second IRQ smoke now also sends one self-targeted SGI through the same
  GIC distributor/CPU-interface path, so we validate that the IRQ route is
  not only alive for the architected timer PPIs but also for software-fired
  private interrupts that look more like later bring-up/debug hooks.
- A matching FIQ smoke now flips that same self-targeted SGI over to
  `Group0 + FIQEn`, keeps ordinary IRQ masked, and proves the FIQ vector can
  take, acknowledge, and EOIR a real GIC-delivered interrupt before we move
  toward board-specific secure/non-secure interrupt routing.
- The timer IRQ evidence is now also reshaped into one explicit `scheduler tick
  ingress` line, so later runtime glue can point to a concrete proof that
  `timer IRQ -> retired interrupt -> ISR-safe Scheduler::tick(now)` is already
  available before we wire the generic scheduler core into this leaf target.
- The same evidence chain now also closes into one `scheduler dispatch` line,
  which says the task-side `svc-trap` path, the ISR-side `timer-tick` path,
  and the cooperative context round-trip can all meet at one future scheduler
  dispatch seam without the generic kernel core knowing anything QEMU-specific.
- The same leaf now also prints one `thread runtime` line that proves the
  lower-half `current` capture, cooperative frame preparation, and already
  observed context-switch round-trip can meet at one reusable runtime seam
  before the upper thread/runtime port starts binding to this leaf.
- The returning SVC path now also records the 24-bit service tag and the
  trap-time `r0-r3` values into one explicit `runtime trap ingress` contract,
  so later `yield / sleep / syscall-like` runtime glue can bind to a proven
  task-side trap envelope instead of treating SVC as a black box.
- The same leaf now also prints one `runtime trap frame` line that proves the
  live `Armv7aExceptionFrame + handler CPSR + SVC instruction word` sample is
  already being captured through a common contract before the later ingress,
  mapping, and writeback layers consume it.
- The same QEMU leaf now also turns those live SVC observations into one
  explicit `runtime trap mapping` line, so we can see the lower-layer
  `svc immediate / origin psr / return-pc / event payload` bundle land on the
  generic trap-frame shape before any future real trap ingress adapter starts
  mutating real exception frames.
- The same leaf now also prints one `runtime trap adapter` line that proves
  the current ARMv7-A SVC path can take that mapped trap shape, treat `r0` as
  the result register, and preserve `lr/spsr` while preparing a future
  `apply_result(frame, result)` ingress seam.
- The same leaf now also prints one `runtime trap seam` line that closes the
  live `svc frame -> mapped generic view -> r0 writeback` path into one
  lower-half contract, so a later upper `RuntimeTrapFrameAdapter` can bind to
  a proven `capture/apply_result` shape instead of inventing that boundary in
  the dark.
- The same leaf now also prints one `runtime trap live-adapter` line that
  pushes that boundary one step closer to the real exception ingress: the proof
  now goes through an actual `Armv7aExceptionFrame*`-shaped live frame adapter,
  so the later upper `RuntimeTrapFrameAdapter<Frame>` hook no longer needs to
  guess whether the ARMv7-A lower half can capture from and write back to the
  real trap frame shape.
- The same leaf now also prints one `runtime trap ingress-adapter` line that
  turns that live-frame proof into a real bindable adapter shape: one
  `ctx + capture + apply_result` bundle now already exists on the ARMv7-A side,
  and the QEMU leaf proves it can survive a full `capture -> dispatch stub ->
  apply_result` round-trip before the upper runtime starts calling into it for
  real.
- The same leaf now also prints one `runtime trap caller` line that proves the
  reverse direction too: generic `yield/sleep` intent can already be encoded
  into an ARMv7-A SVC call frame, round-trip through the same seam, and satisfy
  the future upper `RuntimeTrapIngressCaller` / `RuntimeTrapCallFrameAdapter`
  expectations for `make_*_frame` and `result_ready`.
- The same leaf now also prints one `runtime trap failure` line that turns the
  most important lower-half negative paths into direct evidence too, so
  `unsupported-service`, `decode-failed`, `writeback-failed`, and the two
  `unbound-adapter` entry points can be distinguished on the real QEMU leaf
  instead of only existing in host-side verifier output.
- The same QEMU leaf now also closes `timer IRQ -> tick handoff`, `SVC #0x43
  -> yield_current`, `SVC #0x44 -> sleep_current_until`, and dispatch
  readiness into one `runtime bridge` line, so the lower half can align with
  the upper `runtime_glue` seam without either side needing to know the
  other's QEMU-specific details.
- The same leaf now also prints one `runtime loop ingress` line that lifts
  that bridge evidence to the actual lower-half handoff shape we expect to
  bind later: `tick / isr-defer / bootstrap-idle / bootstrap-worker /
  run-once-or-idle` can now be discussed as one `RuntimeLoopPort`-shaped
  ingress seam, while still keeping the generic runtime semantics verified
  separately on the host side.
- The same leaf now also prints one `runtime leaf ports` line that turns the
  live QEMU session into a real reusable lower-half package instead of only a
  read-only summary: one shared session-local bundle now carries
  exception/interrupt/timer/context/current ingress, the interrupt hook, the
  trap dispatch port, the task-side trap-call port, one
  `RuntimeThreadPort`-shaped thread egress seam, and the
  `RuntimeLoopPort`-shaped runtime loop contract through one explicit
  leaf-owned surface before we ask any later board port to bind against it.
- The same leaf now also prints one `runtime leaf bundle` line that finally
  collects the target-side payload we want future Cortex-A leaves to hand
  upward: exception/interrupt/timer/context/current ingress, one runnable
  `RuntimeLoopPort`-shaped lower-half seam, one proven trap dispatch seam,
  one task-side trap-call seam, one `RuntimeThreadPort`-shaped thread egress,
  and one already-live runtime path. That line now also proves the bundle is
  exported straight from the live leaf-owned ports/runtime state instead of
  being rebuilt locally. That gives us a single QEMU-visible bundle shape to
  preserve when we later swap `virt` out for RK3506.
- The same leaf now also prints one dedicated `runtime thread port` line that
  separates this thread-side egress seam from the wider leaf package: we can
  now see directly that the exported `RuntimeThreadPort`-shaped surface is
  backed by the proven SVC caller path, shares the same leaf context as the
  lower trap-call seam, and stays regression-testable on its own.
- The same leaf now also prints one `runtime binding bundle` line that turns
  the upper-runtime-facing subset into an explicit contract: `current`,
  `trap dispatch`, `runtime thread`, and `runtime loop` now exist as one
  smaller bindable package, with the already-live QEMU runtime path folded in
  as the final readiness gate before any future upper runtime glue binds to it,
  while also proving that this smaller package is exported straight from the
  leaf-owned ports/live state instead of being another hand-written summary.
- The same leaf now also prints one `runtime package` line that puts the two
  previous layers together into one board-facing payload: the wider leaf
  bundle and the smaller runtime-facing binding slice now have to be ready at
  the same time, and the binding slice has to prove it is derived from the
  exported leaf bundle instead of drifting into a second independent shape.
- The same handoff phase now also prints one `runtime handoff` line that
  proves the board-facing payload survives into the actual prepare stage:
  exported runtime package, handoff context, and handoff hook table now have
  to line up as one seam before the prepare report is allowed to count as
  ready.
- The same handoff phase now also prints one `handoff entry` line that proves
  the next-image entry seam is no longer just an implied branch target:
  exported runtime handoff payload, entry address, current branch mode, low
  vectors, translation state, cache state, and masked IRQ/FIQ state now have
  to line up together before this entry surface counts as ready. On the
  current QEMU leaf that branch mode is still the live `sys` mode we are
  running in today, which keeps the log honest until a later real branch shim
  deliberately normalizes it to something else such as `svc`.
- A second handoff-side `handoff transfer` line now pushes that proof one step
  closer to a real branch shim: the exported runtime handoff payload is now
  shaped as the `arg0` value we would actually pass forward, the inherited
  stack pointer is checked at the live transfer site, and the current branch
  state is forced to stay honest about still being `arm/sys` on this QEMU
  leaf. That gives us one explicit pre-branch seam to preserve when we later
  replace this pre-launch proof with a real jump/relocation path.
- A third handoff-side `handoff launch` line now turns that pre-branch seam
  into a real returnable branch probe: the ready transfer payload is handed to
  one explicit launch hook, a tiny assembly shim really enters a trampoline
  dispatch target with the inherited `r0/sp` shape, and the contract now says
  out loud which dispatch target and return site belong to that route. The log
  only goes green once that full launch contract is the one the hook actually
  consumed, and once route-shape, forwarded next-image target, probe capture,
  branch-state, helper return-link, and post-return state all still match it.
  This keeps the seam honest without pretending we already jump into the real
  next image.
- The landing side now also prints one `runtime handoff landing` line that
  turns the post-branch runtime consumer into its own seam instead of leaving
  it buried inside `handoff_live.cpp`: the re-armed leaf ports, the handed-over
  runtime package payload, the recaptured binding slice, and the re-consumed
  live runtime all have to line up before the landing side counts as ready.
- The same landing path now also prints one `runtime handoff package landing`
  line that rebuilds the runtime package directly from the re-armed leaf ports
  and the re-consumed live bit, then requires that rebuilt package to match
  both the handed-over payload and the recaptured local package before the
  landing-side consumer seam counts as ready.
- The same landing path now also prints one `runtime handoff leaf landing`
  line that isolates the wide lower-half bundle seam: the re-armed leaf ports,
  the re-consumed live bit, the handed-over leaf payload, and the recaptured
  local leaf bundle all have to line up before the landed leaf counts as ready.
- The same landing path now also prints one `runtime handoff binding landing`
  line that isolates the smaller runtime-facing binding slice: current/trap/
  thread/loop ports must stay live, point at the same runtime context, still
  derive from the trap-call surface, and match both the handed-over binding
  payload and the recaptured local binding before the landed binding counts as
  ready.
- The same landing path now also prints one `runtime handoff session landing`
  line that finally makes the landed runtime identity explicit: the live
  runtime must expose one non-null `runtime_context/session/shared/trap`
  identity, the re-armed leaf ports must still point at that same runtime
  context, the rebuilt binding slice must still point at that same runtime
  context, and the recaptured local runtime package must agree with it too
  before the landed session counts as ready.
- The same landing path now also prints one `runtime handoff landing bundle`
  line that lifts the whole landed leaf/package slice into one reusable seam:
  landed leaf, landed binding, landed session, rebuilt landed package, and
  the top-level landing observation must all agree on the same recaptured
  payload and re-consumed live runtime before the bundle counts as ready.
- The same landing path now also prints one `runtime handoff consumer` line
  that lifts the whole re-entry sequence into one reusable seam: re-armed
  ports, recaptured package observation, landed bundle seam, live runtime, and
  the full handoff path now have to line up together before the runtime
  consumer counts as ready. The consumer keeps only a compact readiness summary
  of the landed bundle instead of embedding the full nested landing evidence
  again, so the QEMU leaf stays honest about bare-metal stack pressure while
  the detailed bundle line remains available for diagnosis.
- The same live preset now also prints one `runtime handoff path` line that
  closes the whole handoff route into one continuity check: exported handoff
  payload, prepared entry seam, forwarded transfer payload, live launch
  landing, rebuilt post-landing package seam, and runtime consumer all have to
  point at the same runtime handoff object before the path counts as ready.
- The same leaf now also prints one `task syscall frame` line that proves the
  real live `SVC #0x45/#0x46` frame already carries a stable capture-side
  boundary: raw service id, mapped generic service, and current task/stack
  context now meet in one place before any future upper
  `TaskSyscallFrameAdapter<Frame>` starts binding to it.
- The same leaf now also prints one `task syscall dispatch` line that proves
  those same `SVC #0x45/#0x46` requests already reach a stable lower dispatch
  seam with mapped generic service ids, propagated task/stack context, and the
  expected result value written back to `r0`.
- The same leaf now also prints one `task syscall ingress-adapter` line that
  proves the real live `SVC #0x45/#0x46` frame can already be captured through
  the same `live frame adapter` shape, with current task/stack context folded
  into the mapped frame before the result is written back to `r0`.
- The same QEMU leaf now also prints one `task syscall caller` line that proves
  the caller-side `SVC #0x45/#0x46` shape can be synthesized from the lower
  half too, so `debug_write` and `capability_call` now reuse the same
  `svc call frame -> trap seam -> r0 result` evidence path instead of only
  existing as live-dispatch observations.
- The same leaf now also prints one `task syscall roundtrip` line that proves
  the live `SVC #0x45/#0x46` return path closes too, so the value observed
  by the caller after exception return matches both the lower dispatch result
  and the expected syscall semantics for `debug_write` and `capability_call`.
- The same leaf now also prints one `task syscall glue` line that proves the
  new target-side adapter-of-adapter seam can survive inside the real QEMU
  bare-metal image too: generic trap capture, task-syscall ingress,
  task-syscall frame bridge, caller-side frame synthesis, and the generic
  `TaskRuntimeApi` facade now close in one place without linking the full
  shared runtime target.
- The same leaf now also prints one `task syscall failure` line that turns the
  most important negative paths into first-class evidence too, so
  `decode-failed`, `unsupported-service`, `unbound-bridge`,
  `unbound-adapter`, and `writeback-failed` can be distinguished on the real
  bare-metal leaf before they show up during later scheduler/runtime bringup.
- Those returning SVC/IRQ/FIQ smoke lines now also print the pre-exception
  `origin-mode` captured from `SPSR` and the live `handler-mode` read from
  `CPSR`, so banked-mode routing mistakes become visible before we move from
  QEMU toward real Cortex-A silicon.
- An optional `special-irq-smoke` preset now also synthesizes one IRQ-frame
  entry while the GIC CPU/distributor interfaces are live but no line is
  pending, which turns the GIC `special/spurious acknowledge` path into a
  first-class observable contract instead of leaving it hidden behind a later
  timeout.
- A second optional `sgi-irq-timeout-smoke` preset now keeps CPU IRQ masked
  on purpose after a self-targeted SGI has already become pending in the GIC,
  so the timeout path records both the controller-pending evidence and the
  masked CPU route instead of only printing a cleanup-time postmortem.
- A third optional `unexpected-irq-smoke` preset now routes one real
  self-targeted `SGI intid=2` through the normal IRQ path, so the
  `unexpected-intid` handler contract is exercised by a real GIC-delivered
  interrupt instead of only existing as a defensive log branch.
- A fourth optional `sgi-fiq-timeout-smoke` preset now keeps CPU FIQ masked
  on purpose after a Group0 self-targeted SGI is already pending in the GIC,
  so the Group0 + FIQ route has the same dedicated timeout evidence that the
  IRQ route already has.
- The same returning paths now also print one `return evidence` line after the
  handler returns to ordinary execution, comparing the pre-exception `SPSR`
  against the live post-return `CPSR`. That gives us direct evidence for
  `return-pc`, restored mode, restored mask bits, and the stack that the CPU
  came back to after leaving `svc/irq/fiq`.
- The returning and fatal exception paths now also print one `handler stack`
  line that shows the live `SP`, the linker-defined stack range for the
  current mode, and whether `SP` landed inside that banked stack. That gives
  us direct evidence that `svc/irq/fiq/und/abt` are using the per-mode stacks
  initialized in `startup.S`, not accidentally reusing the system stack.
- Timer IRQ and self-SGI smoke now also log one masked `pending evidence`
  snapshot before unmasking CPU IRQ/FIQ, so we can distinguish "the timer or
  SGI already reached the GIC as pending" from "the CPU later took the
  handler" when bring-up moves beyond QEMU.
- We still deliberately skip direct `SCR/NSACR` reads in the default runtime.
  Instead the example now prints one `security side evidence` line that
  summarizes only what was safely observed from the live returning paths:
  which architected timer PPI fired, which handler modes actually ran, and
  whether monitor mode showed up anywhere in those observed paths.
- The boot banner now also prints `ID_PFR1` capability bits for Security,
  Virtualization, and the generic timer. That line is about CPU capability
  only; it is intentionally separate from the later runtime `security side
  evidence` line that talks about observed world/routing behavior.
- The same banner now also prints `ID_MMFR0` capability bits for `VMSA` and
  `PMSA`, so the logs say explicitly whether the CPU model claims an MMU-style
  virtual memory system, an MPU-style protected memory system, or both before
  we interpret any later page-table and abort behavior.
- Current QEMU `-cpu cortex-a7` runs report `ID_MMFR0=0x10101105`, with
  `VMSA=0x5` and `PMSA=0x0`. That matches the direction we are taking here:
  this CPU model advertises virtual-memory/MMU-style machinery, while not
  advertising a PMSA/MPU-only profile.
- Current QEMU `-cpu cortex-a7` runs report `ID_PFR1=0x00010001`, so the model
  advertises the generic timer but not the Security/Virtualization capability
  bits. That makes the later `security side evidence` line even more useful:
  it documents the runtime routing we actually observed, without pretending
  this QEMU CPU model is a full stand-in for a secure-world-enabled SoC.
- The GIC register layer and the interrupt-smoke handler/state layer now live
  in dedicated leaf helpers (`armv7a_gic.cpp` and
  `armv7a_interrupt_smoke.cpp`), so `irq_timer.cpp` stays focused on how each
  smoke is triggered instead of also owning the whole IRQ/FIQ receive path.
- The QEMU `virt` platform interrupt layer now also decodes the per-line GIC
  bits and highest-pending state before handing them to upper layers, so
  `main.cpp` and `irq_timer.cpp` no longer need direct GIC intid constants
  just to print reset or pending evidence.
- QEMU `virt`-specific platform facts now also funnel through
  `armv7a_platform.hpp` and `qemu_virt_platform.cpp`, which centralizes MMIO
  bases, RAM extents, and the probe/abort alias-window layout instead of
  scattering those addresses across every probe and bring-up helper.
- Boot page-table bring-up now includes a tiny coarse L1 -> small-page L2
  path in an otherwise unmapped alias window, which gives us a 4KB-granular
  probe without disturbing the 1MB section identity map used by the default
  runtime.
- That same small-page path now also includes one runtime remap probe, so we
  can rewrite an active L2 entry, invalidate the matching TLB entry by VA,
  and prove that the new 4KB target becomes visible before we take on more
  realistic cache maintenance and board-specific bring-up.
- A dedicated reserved RAM window is now also carved out for one attribute
  probe page, so we can flip a live small-page alias between `normal-cached`
  and `device` without leaving a competing identity mapping behind for the
  same physical page.
- That attribute probe then rewrites one live L2 entry between those two
  memory types at runtime, confirms the data path still behaves across each
  transition, and logs the decoded `TEX/C/B` view that later D-cache work
  will need to reason about.
- A dedicated executable small-page alias now also remaps one live virtual
  address between two different code pages after `SCTLR.I` is enabled, which
  gives us a direct QEMU smoke for instruction-side TLB/I-cache/branch
  predictor maintenance before we move on to anything D-cache related.
- A first cache helper layer now also reads `CLIDR/CCSIDR`, invalidates the
  L1 data cache by set/way before `SCTLR.C` comes up, and exposes line-based
  clean/invalidate helpers for later runtime mapping changes.
- A dedicated D-cache probe then writes through one cached alias, cleans and
  invalidates that line, flips the same VA to `device`, confirms the write
  became visible beyond the cache, writes again via the device view, flips the
  page back to cached, and finally invalidates the cached line before proving
  the device-side write is visible through the cached mapping too.
- Runtime mapping changes now also carry the descriptor address all the way
  into the MMU sync helper, so once `SCTLR.C` is on we clean+invalidate the
  touched page-table cache line before invalidating the matching TLB entry.
- A dedicated page-table probe now keeps its data path `device`-typed while
  `SCTLR.C` is on, remaps one live alias between two different physical pages,
  and proves that the page walker sees the updated L2 descriptor without
  mixing the result together with ordinary cached data traffic.
- A dedicated section-split probe now reserves its own 1MB physical window,
  starts from one live `device`-typed section mapping, then splits that live
  L1 section into a coarse L2 table at runtime and remaps just one 4KB page.
- This gives us a direct QEMU check for the next level up in the translation
  hierarchy: not just "an active L2 entry changed", but "an active section
  mapping was structurally converted into paged mappings while D-cache stayed
  enabled".
- Optional `data` / `prefetch` abort smokes now reuse the shared fatal
  exception path to validate `DFSR/DFAR/ADFSR` and `IFSR/IFAR/AIFSR`
  collection without destabilizing the default CI smoke.
- The `prefetch-xn` smoke adds one XN alias in `domain1 client`, which lets
  us validate an execute-never permission abort without changing the default
  `domain0 client` bring-up path used by the regular smoke.
- The `prefetch-page` smoke keeps a valid coarse L1 entry but leaves the
  matching 4KB L2 entry empty, which lets us validate a true page
  translation fault on the execute path instead of another 1MB miss.
- The `prefetch-page-xn` smoke keeps both coarse L1 and small-page L2 valid,
  but marks the page itself XN in `domain1 client`, which lets us validate a
  true page permission fault on the execute path.
- The `prefetch-page-xn-runtime` smoke starts from a valid executable small
  page in `domain1 client`, executes it once successfully, then flips the same
  L2 entry to XN at runtime and proves the next fetch trips a real page
  permission abort.
- The `data-perm` smoke adds one no-access data alias in `domain1 client`,
  which lets us validate a write-side permission abort while keeping the
  default `domain0 client` runtime unchanged.
- The `data-page` smoke keeps a valid coarse L1 entry but leaves the matching
  4KB L2 entry empty, which lets us validate a true page translation fault
  instead of another 1MB section miss.
- The `data-page-perm` smoke keeps both coarse L1 and small-page L2 valid,
  but marks the page itself no-access in `domain1 client`, which lets us
  validate a true page permission fault on the write path.
- The `data-page-perm-runtime` smoke starts from a writable small page in
  `domain1 client`, proves one write succeeds, then rewrites the live L2 entry
  to no-access and confirms the next write faults with a real page permission
  abort.
- Fatal exception logs now distinguish the pre-abort mode captured in `SPSR`
  from the current handler mode in `CPSR`, which makes abort bring-up logs
  much less ambiguous when we start chasing real board faults.
- Fatal exception logs now also walk the active TTBR0 L1 entry for the fault
  address, so an unmapped hole is visible immediately instead of having to
  infer it indirectly from the syndrome alone.
- When a fault lands in a coarse L1 page-table entry, the same fault log now
  also decodes the matching L2 entry, so later 4KB-page bring-up work has the
  same one-shot visibility that section faults already enjoy.
- Those fault logs now also show the raw short-descriptor `AP` bits, so page
  permission flips are visible directly in the dump instead of only through
  the syndrome.
- This gives us a safe Cortex-A bring-up foothold before we start layering
  more of Charm on top or moving toward RK3506-specific work.
