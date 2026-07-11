# Board Backends and BSPs

Board backends bind Charm to real hardware.

The BSP owns the board facts and hardware-specific binding. It must not redefine
the common backend contract.

Board backend work exposes capability provider instances. The provider instance
is the profile binding target; UART routes, DMA channels, HAL handles, pinmux
facts, and endpoint names are provider evidence or adapter details.

## Reference evidence

`board_reference.hpp` is the v0 real-board evidence reference. It does not drive
hardware, include HAL headers, or claim the H747 board is ready. It records
candidate provider instances and board facts in the common
`BackendEvidenceView` shape.

The current reference intentionally keeps required clock and IRQ facts as
unknown or missing. That proves a real-board backend can export observed H747
facts without turning partial bring-up evidence into readiness.

Validate it with:

```powershell
cmake -S Backends/board/reference_smoke -B Backends/board/reference_smoke/cmake-build-backends-board-reference-smoke -G Ninja
cmake --build Backends/board/reference_smoke/cmake-build-backends-board-reference-smoke
ctest --test-dir Backends/board/reference_smoke/cmake-build-backends-board-reference-smoke --output-on-failure
```

## BSP responsibilities

- SoC and board identity.
- Clock sources and relevant clock tree facts.
- Pinmux routes.
- Early console route.
- IRQ controller and interrupt line facts.
- Memory regions.
- Startup and linker-script inputs.
- Vendor SDK or HAL binding.
- Board evidence such as register readback, probe results, and bring-up logs
  presented as structured facts.

The BSP may use HAL or vendor SDK code, but HAL is not the Charm backend
language shared with host and QEMU.

## Relationship to targets

`targets/` remains the build leaf area. A target may select a board BSP, but the
BSP is not the same thing as the target.

For example, `targets/rk3506` may continue to own the current RK3506 build leaf
while future work extracts reusable board-backend facts under `Backends/board/`.
