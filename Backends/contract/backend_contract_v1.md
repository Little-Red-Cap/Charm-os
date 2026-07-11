# Backend Contract Candidate v1

## Purpose

Backend Contract Candidate v1 promotes the proven v0 topology/evidence boundary
into two concrete low-risk capability slices:

- `console/output`
- `block/storage`

It is still a contract candidate. It is not a public `Modules/` API, not a HAL,
not a BSP, not a service locator, and not a manifest/generator system.

## Base Rule

```text
Applications declare capability requirements.
Profiles bind requirements to provider instances.
Providers adapt backend resources into stable capability contracts.
```

Bindings target provider instances only. Provider types, adapters, transports,
HAL handles, endpoint objects, backend tokens, and file paths are not binding
targets.

## v1 Contract Headers

- `capability_topology.hpp`: requirement, provided token, provider instance,
  binding, metadata, and runtime context topology.
- `backend_evidence.hpp`: backend identity, capability exports, selected
  binding evidence, facts, and readiness summary.
- `console_output.hpp`: `ByteSink`, `TextSink`, `LineSource`, console roles,
  transfer/status result, and structured console provider evidence.
- `block_storage.hpp`: `BlockDevice`, block roles, provider-published
  `BlockEndpoint`, status result, and structured block provider evidence.

## Migration Decision Table

| item | v1 classification | allowed destination | rule |
| --- | --- | --- | --- |
| `TextSink` / `LineSource` | capability contract candidate | `Backends/contract/console_output.hpp` | App/domain may consume these roles through requirements. |
| UART / COM port / DMA channel | provider evidence or adapter detail | backend provider/adaptation only | Must not enter app/domain dependency or binding target. |
| H747 status line text | presentation | system smoke or board evidence presentation | It may project into structured evidence, but is not schema. |
| `BlockDevice` | capability contract candidate | `Backends/contract/block_storage.hpp` | App/runtime may consume block semantics through a requirement. |
| `BlockEndpoint` | provider-published consumption surface | `Backends/contract/block_storage.hpp` | It is not provider instance identity and cannot be a binding target. |
| QSPI / eMMC / file handle | provider evidence or adapter detail | backend provider/adaptation only | They explain implementation and readiness, not app semantics. |
| Store v1 / FAT path / ResourcePack | higher-layer resource/runtime vocabulary | outside v1 contract headers | They may consume block capability but must not redefine it. |
| provider instance identity | binding/evidence target | profile/evidence/explain surface | It may be selected by profile but must not leak into app/domain code. |

## v1 Validation Gate

Run:

```powershell
.\Backends\run-backends-v1-smoke.ps1
```

The gate must include:

- contract topology header smoke
- backend evidence header smoke
- console/output header smoke
- block/storage header smoke
- host reference backend smoke
- QEMU reference backend smoke
- board reference evidence smoke
- system topology bridge smoke
- system console/output provider smoke
- system block/storage provider smoke

## Non-Goals

- Do not move `Modules/platform` code.
- Do not promote these headers to `Modules/`.
- Do not define display/audio/M4 proxy in v1.
- Do not define Store v1, FAT path, ImageStore, ResourcePack, ELF, or ModuleX
  as backend contract concepts.
- Do not require QEMU to emulate H747 external peripherals.
