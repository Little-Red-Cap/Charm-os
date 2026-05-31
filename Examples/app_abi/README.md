# Charm App ABI Prototype

This directory holds the first-generation dynamic App ABI prototype shared by
host smokes, H747 resident runtime experiments, and embedded App ELF samples.

The ABI is intentionally C-compatible:

```c
extern "C" int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

It is not a C++ ABI, service locator, manifest language, or final public
runtime framework. The first contract is a narrow capability table for real app
needs: console, time, display, input, optional storage, reserved AFE, and app
exit/return-code recovery.

`charm_app_runtime.hpp` is a reusable prototype for the resident-runtime side of
the same contract. It centralizes image lookup, image loading callbacks, API
validation, `argc/argv` construction, optional execute-ready preparation, entry
invocation, and staged diagnostics (`lookup/load/abi/argv/start/exit`). Its
`prepare()` path stops at the `start` stage and materializes the loaded entry
plus argv without calling target code; `run()` is the explicit execution path.
It intentionally remains under
`Examples/app_abi` until the shape has survived H747, host, and ModuleX pressure.

`charm_app_staged_runtime.hpp` is the shared adapter for already-staged images.
QSPI/eMMC App Store paths and Dev Loader received-image paths both converge on
`AppImage -> staged AppImageSource -> AppRuntime`; the adapter only wraps one
`AppImage` plus the caller-provided loader callback and does not define a new
image format, entry point, or capability table.

POSIX remains a compatibility layer for C/POSIX programs. Player, Scope, and
future hot-loaded apps should target this capability-table semantic model first;
ELF is the first image format, and ModuleX must later reuse the same entry and
capability table rather than defining a second program model.

`charm_app_modulex_loader.hpp` is the first host-only ModuleX App loader
prototype. It treats ModuleX as a second `AppImage` format, resolves
`charm_app_main` from the ModuleX symbol table, and materializes the result as
the same `CharmAppMainFn` consumed by `AppRuntime`. It deliberately does not
reuse the POSIX ModuleX `main(argc, argv, envp)` ABI. Because current GCC
modules and libstdc++ text headers are sensitive to include/import order,
callers include the normal App ABI headers and standard library headers first,
then import `module_core/module_view/module_loader/module_link`, then include
`charm_app_modulex_loader.hpp`.

`player_min_core.h` holds the first shared Player-mini app core used by both the
host smoke and the embedded App ELF sample. It is deliberately small: query the
display, poll input, get time, submit one bounded ARGB8888 raster payload, and
return an exit code.

`charm_app_store.hpp` defines the first read-only program-image store layout for
external media such as H747 QSPI Nor Flash. The store is intentionally minimal:
a fixed header plus fixed entries that map an app name to an ELF byte range.
It is not a filesystem, manifest DSL, or package manager.

The same header also contains the board-free store builder and staging helpers.
The builder creates an in-memory v1 store image from named payloads, while the
staging helper turns `AppStoreReader + name/raw range + cache buffer` into an
`AppImage` ready for the resident runtime. This keeps QSPI, future eMMC slots,
and test fixtures on one program-store shape without adding CRC, signatures, or
slot policy yet.

The resident platform boundary is now shared by both development downloads and
external stores: USB/UART packetstream receive produces verified bytes, QSPI or
future eMMC store readers produce named byte ranges, and both must converge on
`AppImage -> staged AppImageSource -> AppRuntime`. Store v1 remains only
`header + entries + payload`; it is not a filesystem or product update slot
format. On H747, large receive/stage caches may live in SDRAM, but executable
App ELF loading still uses the board-owned execution region selected by the
runtime backend; cache placement must not change the `AppImage` or AppRuntime
contract.

`elf_samples/build_app_store.ps1` builds the current sample App ELFs and packs
them into a development `.appstore.bin` file. That file is the host/QSPI/eMMC
preparation artifact for this prototype stage; it is not a final product update
bundle.

`charm_app_store_install.hpp` defines the board-free install semantics for
flash-like media. It models erase/write/read/capacity/alignment and verifies a
written `.appstore.bin` by readback. The host smoke uses a memory NOR simulator;
H747 `dev_loader` now binds the same installer contract to QSPI NOR for the
resident development platform, and also binds eMMC as a second raw development
slot through the same reader/media contracts. The eMMC adapter is block-backed
internally, but still presents Store v1 as byte ranges to the runtime. It must
not define a second App Store protocol, filesystem convention, or app entry
model.

`charm_app_received_image.hpp` defines the first received-image staging boundary
between Dev Loader downloads and `AppRuntime`. It turns verified image bytes
into an `AppImage` without defining a second entry point, loader, or app model.

`charm_app_elf_probe.hpp` verifies received App ELF load semantics without
executing target code on the host. It checks ELF32 load segments, entry offset,
load span, buffer alignment/capacity, and minimum diagnostics for H747 handoff.
It also exposes a disabled run-plan helper plus an `AppImageSource::load`
compatible ELF loader backend that materializes the would-be `LoadedAppImage`
entry address after a successful dry load. Callers must still decide explicitly
when running is enabled.

Current store validation entry points:

- `Examples/system/app_abi_store_smoke`
- `Examples/system/app_abi_store_pack_smoke`
- `Examples/system/app_abi_store_file_smoke`
- `Examples/system/app_abi_store_install_smoke`
- `Examples/system/dev_loader_store_receive_smoke`
- `Examples/system/dev_loader_store_install_handoff_smoke`
- `Examples/system/dev_loader_app_handoff_smoke`
- `Examples/system/dev_loader_received_elf_smoke`
- `Examples/system/app_abi_modulex_smoke`
- `Examples/system/dev_loader_received_modulex_smoke`
