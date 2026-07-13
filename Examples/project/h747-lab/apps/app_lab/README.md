# H747 App Lab

`app_lab` is the embedded-image baseline for the Charm App ABI on H747. It is
separate from the resident download path in `dev_loader` and the C/POSIX program
path in `posix_lab`.

```c
int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

The target embeds known App ELF and Store v1 fixtures so loader, ABI, argv,
capability and QSPI behavior can be checked without first depending on a host
packetstream transfer. It is not the owner of resident download/install policy.

## Monitor Commands

- `app list`: list embedded App images.
- `app run <name> [args...]`: load an embedded ELF and call `charm_app_main`.
- `app store install`: install the embedded Store v1 image at QSPI offset zero.
- `app store status|list`: inspect the installed QSPI Store.
- `app run-path qspi:<name> [args...]`: stage a named Store image and run it.
- `app run-path qspi:@<offset>:<size> [args...]`: stage a raw QSPI ELF range.
- `app run-path <other>`: stable file-backed `not_supported` path.
- `app status`: print monitor, source/store, install, run, ELF-load and capability
  diagnostics.
- `app smoke`: run the embedded, QSPI and unsupported-path baseline.

## Fixtures

- `hello_app` verifies C ABI entry, console, argv and exit recovery.
- `player_min` verifies the diagnostic display/input/time capability surface.

These are platform fixtures, not product UI. `player_min` uses a bounded raster
payload and does not define a full-frame memory policy.

## Runtime Boundaries

- Firmware startup loads the resident monitor; the monitor loads App ELF.
- Embedded and QSPI images converge on `AppImage -> staged AppImageSource ->
  ELF loader -> AppRuntime -> CharmAppApi`.
- QSPI lookup and staging use the shared Store v1 helpers under
  [`Examples/app_abi`](../../../../app_abi/README.md); H747 supplies only media,
  execution memory, cache preparation and capability bindings.
- The dynamic ABI is C-compatible and does not expose C++ classes, exceptions,
  RTTI, vtables or source-level concepts.
- Generic filesystem-backed executable loading remains unsupported.
- ModuleX, when used by the resident platform, remains another image format for
  the same App ABI; `app_lab` does not define a second entry model.

The target's fixed ELF load region starts at `0x24070000`. Image link address,
load buffer and cache preparation must remain consistent. Exact Store fields,
default fixtures and status tokens come from source and the capture script, not
from a copied struct in this README.

## Validation

Build、flash、capture、manual commands、token validation 与 retained board evidence 只在
[`h747_lab_app_lab_smoke.md`](../../docs/h747_lab_app_lab_smoke.md) 维护。三个动态入口的当前分工见
[`h747_lab_dynamic_boundary_roadmap.md`](../../docs/h747_lab_dynamic_boundary_roadmap.md).
