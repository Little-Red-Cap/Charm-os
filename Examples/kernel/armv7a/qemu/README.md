# ARMv7-A QEMU bare-metal skeleton

This is the first Cortex-A oriented leaf target for Charm.
It keeps startup, linker, and early UART code inside the example target
instead of pushing ARMv7-A specifics into shared `Modules/`.

## Build

```powershell
cmake --preset debug
cmake --build out\build\debug --verbose
```

Abort smoke presets stay separate so the default IRQ/SVC smoke remains stable:

```powershell
cmake --preset debug-abort-data
cmake --build out\build\debug-abort-data --verbose

cmake --preset debug-abort-prefetch
cmake --build out\build\debug-abort-prefetch --verbose
```

## Run

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
ARMv7-A cp15 state, sctlr=0x00C50078, vbar=0x40200000, mpidr=0x80000000, cntfrq=0x03B9ACA0
ARMv7-A cache state, mmu=off, dcache=off, icache=off, high-vectors=off
ARMv7-A translation state, ttbr0=0x00000000, ttbr1=0x00000000, ttbcr=0x00000000, dacr=0x00000000
ARMv7-A L1 table ready, base=0x40208000, ram=0x40211C0E, gic=0x08010C16, uart=0x09010C16
ARMv7-A MMU active, sctlr=0x00C50079, ttbr0=0x40208000, ttbcr=0x00000000, dacr=0x00000003
ARMv7-A MMU flags, mmu=on, dcache=off, icache=off
Charm out.format import active, PL011 @ 0x09000000
ARMv7-A SVC vector active, imm=0x000043
ARMv7-A timer IRQ active, intid=30
```

## CI smoke

```powershell
.\run_qemu_ci.ps1
```

Abort smoke CI is intentionally separate because these runs end in the fatal
exception path instead of returning to the regular SVC/IRQ smoke:

```powershell
.\run_qemu_abort_ci.ps1 -Kind data
.\run_qemu_abort_ci.ps1 -Kind prefetch
```

## Abort smoke

Use the dedicated preset and pass its ELF to `run_qemu.ps1`:

```powershell
.\run_qemu.ps1 -ElfPath out\build\debug-abort-data\charm-armv7a-qemu
.\run_qemu.ps1 -ElfPath out\build\debug-abort-prefetch\charm-armv7a-qemu
```

Expected abort-mode output includes the same boot/MMU banner as the default
smoke, then stops in one of these fault paths instead of printing the later
SVC/IRQ lines:

```text
ARMv7-A abort smoke, kind=data, addr=0x20000000
ARMv7-A exception: data abort, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A data fault, dfsr=0x........, dfar=0x20000000, adfsr=0x........
ARMv7-A data fault decode, status=0x05 (section translation fault), domain=0x0, write=no, cm=no
```

```text
ARMv7-A abort smoke, kind=prefetch, addr=0x20000000
ARMv7-A exception: prefetch abort, pc=0x20000000, lr=0x20000004, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A prefetch fault, ifsr=0x........, ifar=0x20000000, aifsr=0x........
ARMv7-A prefetch fault decode, status=0x05 (section translation fault), domain=0x0
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

- The linker starts at `0x40200000` to stay clear of the `virt` DTB area
  near `0x40000000`.
- Current scope is intentionally small: reset entry, per-mode stacks,
  VBAR/vector setup, one returning SVC smoke, one returning timer IRQ
  smoke, and early PL011 UART on QEMU `virt`.
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
  is now also wired into `TTBR0` with `TTBCR=0`, `DACR=0x3`, and a first
  `SCTLR.M` enable step. The current smoke keeps both I-cache and D-cache off
  so the very first translation bring-up stays easy to reason about.
- The timer smoke prepares both architected physical timer PPIs.
  Current QEMU `virt` runs observed the non-secure physical timer route
  (`intid=30`), but the code also accepts the secure route (`intid=29`)
  so the same leaf target is less brittle across reset states.
- Optional `data` / `prefetch` abort smokes now reuse the shared fatal
  exception path to validate `DFSR/DFAR/ADFSR` and `IFSR/IFAR/AIFSR`
  collection without destabilizing the default CI smoke.
- Fatal exception logs now distinguish the pre-abort mode captured in `SPSR`
  from the current handler mode in `CPSR`, which makes abort bring-up logs
  much less ambiguous when we start chasing real board faults.
- This gives us a safe Cortex-A bring-up foothold before we start layering
  more of Charm on top or moving toward RK3506-specific work.
