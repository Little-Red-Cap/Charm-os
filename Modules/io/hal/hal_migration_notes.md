# HAL Migration Notes (Draft)

## Naming Convention
- All HAL-related modules use hal_* prefix under Modules/.
- Examples go under Draft/Examples/hal_* to keep them separate from main route demos.

## Phase 1 (MVP Interfaces)
- hal_core: Status/Result, tick_t, ClockInfo
- hal_clock: ClockProvider concept
- system.clock: TimeSource (board_caps)
- hal_irq: IrqGuard, IrqController
- hal_gpio: pin/config + GpioDriver concept
- hal_uart: config/handle + UartDriver concept
- hal_timer: config/handle + TimerDriver concept

## Notes
- Interfaces only: no platform implementations in core modules.
- Platform bindings should live in target examples or a future charm-hal repo.
- Keep modules minimal and orthogonal for easy trimming.


