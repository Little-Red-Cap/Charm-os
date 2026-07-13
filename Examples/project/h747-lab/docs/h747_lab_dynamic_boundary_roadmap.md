# H747 Lab Dynamic Boundary Roadmap

> status: `supporting`
>
> scope: current role split between `dev_loader`, `app_lab` and `posix_lab`

H747 has three dynamic-image experiments, but only one resident development
mainline. Current behavior must be read from source, CMake targets and the latest
smoke; this file only prevents the three roles from being conflated.

## Current Role Split

### `dev_loader`: resident App platform mainline

`dev_loader` is the active development path for Apps that use:

```text
packetstream or Store v1
-> AppImage
-> ELF/ModuleX loader
-> AppRuntime
-> charm_app_main(CharmAppApi*, argc, argv)
```

It owns received-image transport binding, SDRAM staging, QSPI/eMMC Store media,
the fixed D1 ELF execution region and monitor diagnostics. It is not a product
bootloader and does not define a second App ABI.

### `app_lab`: embedded-image baseline

`app_lab` keeps the smallest board baseline for the same `CharmAppApi` and
`charm_app_main` model. Embedded `hello_app` and `player_min` images allow loader
and runtime checks without first depending on packet transport or mutable Store
media. It is a baseline and compatibility target, not the owner of download,
install or resident platform policy.

### `posix_lab`: POSIX/C ELF compatibility line

`posix_lab` validates `spawn/load/wait`, fd/path/pipe/stat and C/POSIX program
image behavior. It does not define `CharmAppApi`, `charm_app_main` or the
resident App model. POSIX `main(argc, argv, envp)` and App ABI entry semantics
must remain explicit rather than being silently adapted into one another.

## Shared Boundaries

- `dev_loader` and `app_lab` consume the same `AppImage -> AppRuntime ->
  CharmAppApi` model.
- Store and received payloads converge before the loader; transport/media
  identity does not enter the App ABI.
- ELF remains the primary resident App format. ModuleX is a second image format,
  not a second App model.
- Host/QEMU smokes prove loader/runtime semantics; H747 proves real memory,
  cache, transport and media behavior. One evidence domain cannot replace
  another.
- Player samples exercise the capability boundary but do not own platform
  contracts or peripheral policy.

## Validation Order

For resident App changes, use this order:

```text
artifact validation and host smoke
-> optional QEMU ELF evidence
-> H747 dev_loader build
-> focused received/store board smoke
-> full QSPI/eMMC matrix when required
```

`app_lab` and `posix_lab` are built or run when their own baseline changes; they
are no longer mandatory predecessors for every `dev_loader` iteration.

## Non-goals

- No product boot selection, signature, rollback or crash recovery.
- No private raw jump outside `AppRuntime`.
- No merging of POSIX and App entry ABIs.
- No display, touch or Player UI ownership in these targets.

Detailed entries:

- [`dev_loader`](../apps/dev_loader/README.md)
- [`app_lab`](../apps/app_lab/README.md)
- [`posix_lab`](../apps/posix_lab/README.md)
- [`H747 layering`](h747_lab_layering_contract.md)
