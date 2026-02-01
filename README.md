# Charm-os


## Main Route Demos (Windows)
- M0: `Examples/windows/main.cpp` (kernel + timer + event queue)
- M1: `Examples/windows/main_m1.cpp` (sync + IPC)
- M2: `Examples/windows/main_m2.cpp` (thread + blocking)
- M3: `Examples/windows/main_m3.cpp` (trace + stats)

> Experimental demos are intentionally kept out of the main route.

## Optional Modules (Default OFF)
- dynamic registry: `kernel.dynamic_registry`, `kernel.task_pool`, `kernel.task_auto`
- dynamic priority queue: `kernel.event_queue_list`
- observability: `kernel.trace`, alert/replay, JSON diagnostics
- event policies: dedup/debounce/coalesce/boost, drop policy

## M1 Tests
- Draft/m1_tests.md (minimal sync/IPC checklist)


## M2 Spec
- Draft/m2_thread_spec.md (thread/blocking semantics)
- Draft/m2_api_freeze.md (API freeze)


## M3 Optional
- Draft/m3_observability_plan.md (observability/perf optional plan)


## STM32 Demo
- Location: `Examples/stm32f103c8`
- Demo entry: `main_mcu_stub.cpp` (off by default)
- Enable in CMake: `-DCHARM_MCU_KERNEL_DEMO=ON`
- Port layer is chip-agnostic; STM32 binding is in `Core/Src/kernel.port.stm32.cpp`.

## Platform Port
- Interface: `Modules/port/port.kernel.cppm`
- Template: `Modules/port/port.kernel.template.cpp`
- Windows binding: `Examples/windows/port.kernel.windows.cpp`

