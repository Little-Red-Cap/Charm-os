# H747 Lab Apps

Apps are scenario logic only. They consume `services/*` and are selected by a
profile-backed firmware target.

New reusable app/domain code should depend on `charm::cap` concepts where a
behavior can run on multiple backends. Current examples are stream output/input
and the minimal display fill capability.

Each app directory must provide an `app.cmake` manifest with:

- `H747_LAB_APP_NAME`
- `H747_LAB_APP_SOURCES`
- `H747_LAB_APP_INCLUDE_DIRS`

## Current Apps

- `diag_shell`: board evidence shell. Commands include `status`,
  `power status`, `pmic probe`, `memory status`, `sdram1 probe`,
  `sdram2 probe`, `sdram1 verify`, `sdram2 verify`, `sdram1 bus`,
  `sdram2 bus`, `sdram1 spot`, `sdram2 spot`, `sdram1 alias`,
  `sdram2 alias`, `sdram1 waitbus`, `sdram2 waitbus`, `sdram1 timing`,
  `sdram2 timing`, `qspi probe`, `qspi read`, and `reboot`. `spot` checks
  single-address write/read behavior; `bus` checks simple data-pattern evidence;
  `alias` captures neighboring words around sampled offsets; `waitbus` repeats
  the bus diag after command-busy waits; `timing` sweeps CAS/read-pipe and mode
  register presets. Its console path is adapted through `TextSink` and
  `LineSource`. `memory mpu normal` is an explicit diagnostic escape hatch that
  marks both SDRAM banks as normal non-cacheable memory before rerunning probes;
  it is not a default boot policy.
- `display_demo`: minimal display baseline. It initializes the verified
  HX8394D DSI path and shows a fixed red screen through `SolidFillDisplay`.
- `display_raster_demo`: first capability-world raster demo. The domain code
  targets `RasterDisplayWorld` and can run against both host/mock and H747
  SDRAM framebuffer backends.

## Reserved Apps

- `player`
- `posix_lab`

These directories are placeholders for later Charm integration work and are not
part of the first build baseline.
