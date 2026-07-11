# QEMU Backends

QEMU backends run Charm in a machine model that is closer to bare metal than a
PC host backend.

Their job is to validate startup, exception/trap paths, timer behavior,
interrupt routing, early console, and memory-map assumptions before real board
bring-up.

## v0 responsibilities

- Own QEMU provider instances and their evidence.
- Export early console capability.
- Export timer observation facts.
- Export exception/trap observation facts.
- Export interrupt controller observation facts.
- Export memory map facts.
- Produce evidence that can be compared with real-board bring-up evidence.

QEMU is not a replacement for real peripheral validation. It is the middle rung
between host semantic smoke tests and board evidence.

QEMU provider instances are binding targets. Machine model details, adapters,
trap vectors, HAL-like stubs, and endpoint names are evidence or implementation
details, not app/domain dependencies.

## Reference backend v0

`qemu_reference.hpp` is the first QEMU backend implementation candidate. It is
header-only and intentionally narrow:

- `qemu.early_console` provides an early `TextSink`-style console provider for
  smokes.
- `ReferenceBackend::evidence_view()` exports QEMU backend identity, early
  console capability export, selected binding evidence, and facts for timer,
  trap, IRQ, and memory-map readiness.
- The reference memory map includes the resident ELF QEMU runtime region
  `0x20080000..0x20090000`.

It does not launch QEMU and does not emulate H747 peripherals. DSI/LTDC/eMMC,
GT9xx touch, USB CDC, FMC SDRAM, and board HAL initialization remain real-board
or dedicated QEMU-smoke concerns.

## Current validation entrypoints

- `Backends/qemu/reference_smoke`
- `Examples/system/resident_elf_qemu_smoke`
