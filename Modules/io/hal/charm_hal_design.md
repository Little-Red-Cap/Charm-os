# Charm HAL Design (Draft)

## Goals
- Chip-agnostic interfaces; platform impl lives in targets/examples.
- Small, composable, IDE-friendly; zero dynamic alloc path preserved.
- Contracts first: document blocking/async semantics, ownership, timing.

## Scope (MVP interface set)
- core: time (tick_ms), irq (disable/enable), clock (freq query), delay/sleep.
- io: gpio (in/out, irq edge), uart (tx/rx, irq/dma optional), timer (oneshot/periodic), spi, i2c.
- optional later: flash, entropy, pwm, adc, watchdog.

## Shape
- Module: port.kernel keeps KernelCaps for scheduler (time/irq/wakeup/swi).
- Module: charm-hal.gpio etc. Each module defines:
  - lightweight handle (struct Handle { platform ptr/id })
  - cfg struct (constexpr defaults)
  - ops as free functions or static member of Impl
  - concept checks for Impl
- Capability detection: constexpr bool has_<feature>(Impl) via requires.
- No global macros in interface; selection via CMake/manifest choosing platform impl files.

## Platform binding
- For each target: 	argets/<vendor>/<board>/hal_<periph>.cpp implements the concepts.
- Provide template file Modules/port/port.kernel.template.cpp as quick start; similar templates for gpio/uart.

## Build selection
- CMake cache variables choose target; a manifest lists sources to compile for that target.
- Upper layers link only the interfaces they use (per-module granularity), enabling dead-strip.

## Compatibility layers
- POSIX/Arduino live in charm-shim, depend on charm-hal + charm-os (or charm-rt facade), not the other way around.

## RT facade
- charm-rt: optional thread API mapping to EDA/ThreadTask; stacks configurable; keeps zero-alloc path available when disabled.

## Documentation
- For each periph: table of required/optional behaviors (blocking/async, timeout units, ISR context rules).
- Example usage snippets per module; platform notes (e.g., STM32 requires clock enable before gpio_cfg).

## Phasing
- Phase A (current): port.kernel + minimal time/irq for PC/STM32.
- Phase B: add gpio/uart/timer, publish concepts and template impl.
- Phase C: spi/i2c/flash + shim time/sleep; start RT facade design.
- Phase D: entropy/pwm/adc/watchdog + richer shim (POSIX subset).
