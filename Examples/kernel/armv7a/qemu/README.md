# ARMv7-A QEMU bare-metal skeleton

This is the first Cortex-A oriented leaf target for Charm.
It keeps startup, linker, and early UART code inside the example target
instead of pushing ARMv7-A specifics into shared `Modules/`.

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
```

## CI smoke

```powershell
.\run_qemu_ci.ps1
```

`run_qemu_ci.ps1` now configures and rebuilds the default `debug` preset
before launching QEMU, so the smoke log stays aligned with the current source
instead of whatever ELF happened to be left in `out\build\debug`.

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
- Those returning SVC/IRQ/FIQ smoke lines now also print the pre-exception
  `origin-mode` captured from `SPSR` and the live `handler-mode` read from
  `CPSR`, so banked-mode routing mistakes become visible before we move from
  QEMU toward real Cortex-A silicon.
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
