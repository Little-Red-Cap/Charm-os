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
ARMv7-A L1 table ready, base=0x40210000, ram=0x40211C0E, gic=0x08010C16, uart=0x09010C16
ARMv7-A small-page alias ready, va=0x5200...., pa=0x4020...., l1=0x4020...., l2=0x4020....
ARMv7-A small-page remap ready, va=0x52100000, pa-a=0x4020...., pa-b=0x4020...., l1=0x4021...., l2=0x4021....
ARMv7-A attr probe ready, va=0x52300000, pa=0x40300000, section=0x40300000, identity-l1=0x00000000, l1=0x4021...., l2=0x4030...., tex=0x00000001, mem=normal-cached
ARMv7-A dcache probe ready, va=0x52400000, pa=0x4030...., l1=0x4021...., l2=0x4030....
ARMv7-A icache probe ready, va=0x5220...., pa-a=0x4020...., pa-b=0x4020...., l1=0x4021...., l2=0x4020....
ARMv7-A page-table probe ready, va=0x52500000, pa-a=0x4030...., pa-b=0x4030...., desc=0x4021...., l1=0x4021...., l2=0x4030....
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
.\run_qemu_abort_ci.ps1 -Kind prefetch-xn
.\run_qemu_abort_ci.ps1 -Kind prefetch-page
.\run_qemu_abort_ci.ps1 -Kind prefetch-page-xn
.\run_qemu_abort_ci.ps1 -Kind prefetch-page-xn-runtime
.\run_qemu_abort_ci.ps1 -Kind data-perm
.\run_qemu_abort_ci.ps1 -Kind data-page
.\run_qemu_abort_ci.ps1 -Kind data-page-perm
.\run_qemu_abort_ci.ps1 -Kind data-page-perm-runtime
```

## Abort smoke

Use the dedicated preset and pass its ELF to `run_qemu.ps1`:

```powershell
.\run_qemu.ps1 -ElfPath out\build\debug-abort-data\charm-armv7a-qemu
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
SVC/IRQ lines:

```text
ARMv7-A abort smoke, kind=data, addr=0x20000000
ARMv7-A exception: data abort, pc=0x........, lr=0x........, spsr=0x........, origin-mode=sys, current-cpsr=0x........, current-mode=abt
ARMv7-A data fault, dfsr=0x........, dfar=0x20000000, adfsr=0x........
ARMv7-A data fault decode, status=0x05 (section translation fault), domain=0x0, write=no, cm=no
ARMv7-A fault map, far=0x20000000, ttbr0=0x........, l1[0x200]=0x00000000 (fault)
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
