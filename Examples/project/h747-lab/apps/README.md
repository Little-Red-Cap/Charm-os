# H747 Lab Apps

Apps own scenario behavior. Profiles select an app and its services; board/HAL
ownership remains below service facades.

Each app directory provides `app.cmake` with:

```text
H747_LAB_APP_NAME
H747_LAB_APP_SOURCES
H747_LAB_APP_INCLUDE_DIRS
```

The manifest is the source of build truth. This README does not duplicate source
lists, monitor commands or smoke tokens.

## App Index

| App | Role / detailed entry |
|---|---|
| `app_lab` | embedded-image App ABI baseline; [`README`](app_lab/README.md) |
| `capability_mvp` | small cross-environment capability/source-boundary experiment; [`README`](capability_mvp/README.md) |
| `dev_loader` | resident received/Store ELF and ModuleX platform; [`README`](dev_loader/README.md) |
| `diag_shell` | board, power, memory, QSPI and storage evidence shell |
| `display_demo` | minimal panel/solid-fill baseline |
| `display_raster_demo` | SDRAM framebuffer and raster-present evidence |
| `input_probe` | touch, encoder and button hardware evidence |
| `player` | small H747 Player scenario; [`README`](player/README.md) |
| `player_md3` | shared MD3 Player board integration; [`README`](player_md3/README.md) |
| `posix_lab` | POSIX/C ELF compatibility shell; [`README`](posix_lab/README.md) |
| `resident_launcher` | boot-facing filesystem ELF launcher prototype; [`README`](resident_launcher/README.md) |
| `storage_firmware_runtime` | eMMC and USB MSC evidence app |
| `usb_msc_legacy_probe` | legacy USB MSC comparison/probe target |

## Boundaries

- An app may translate a service result into domain behavior, but does not own
  peripheral initialization, IRQ, DMA, cache or vendor handles.
- Project-local C++ capability concepts are source constraints, not dynamic App
  ABI types.
- Resident Apps enter through `AppImage -> loader -> AppRuntime -> CharmAppApi`;
  no app may add a private raw-jump path.
- Board evidence, monitor output and product behavior remain separate concerns.
- Player-specific UI and controller details belong to the Player entries, not
  this index.

Current dynamic-image roles are defined once in
[`h747_lab_dynamic_boundary_roadmap.md`](../docs/h747_lab_dynamic_boundary_roadmap.md).
Source ownership and prototype admission are defined in
[`h747_lab_layering_contract.md`](../docs/h747_lab_layering_contract.md).
