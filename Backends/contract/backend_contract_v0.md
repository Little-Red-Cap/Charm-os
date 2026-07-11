# Backend Contract v0

## Purpose

Backend Contract v0 defines how Charm describes the execution carrier beneath
`core`, `system`, and `io`.

The contract is not a hardware abstraction layer and not a board package. It is
the narrow boundary that lets host, QEMU, and real-board backends export
capabilities and evidence in comparable forms.

The v0 topology rule is:

```text
Applications declare capability requirements.
Profiles bind requirements to provider instances.
Providers adapt backend resources into stable capability contracts.
```

## Non-goals

- Do not move existing code out of `Modules/platform/` in v0.
- Do not require Linux host support in v0.
- Do not make QEMU the default backend.
- Do not make `targets/` the contract root.
- Do not turn backend resolution into a service locator.
- Do not introduce a manifest, generator, or DSL.

## Vocabulary

`backend`
: The execution resource domain used to run Charm. Examples: `host.win32`,
  `host.linux`, `qemu.armv7a`, and `board.h747`. It is the namespace and
  resource boundary that owns provider instances, adapters, and downstream
  HAL/OS/simulator/file/syscall dependencies.

`runtime domain`
: The execution domain inside a backend where code actually runs. Examples:
  `host_process`, `qemu_m7`, `h747_cm7`, and `h747_cm4`.

`capability`
: An app/domain-consumable semantic contract. Examples: `TextSink`,
  `LineSource`, and `BlockDevice`.

`requirement`
: An app or component declaration that it needs a capability role, such as
  `TextSink.log` or `BlockDevice.app_store`.

`binding`
: A profile result that maps one requirement to one provider instance. It is an
  assembly result, not an implementation.

`provider type`
: A provider implementation family, such as a host buffered console provider or
  host memory block provider. It is metadata and must not be a binding target.

`provider instance`
: The concrete provider selected by a profile binding, such as
  `host.buffered_console`, `h747.usart1.console`, or
  `host.memory_block_app_store`.

`adapter`
: Provider-internal mechanism translation. Examples include a memory-buffer
  console adapter, STM32 UART DMA adapter, or file-backed block adapter. It is
  not the architecture boundary and must not be a binding target.

`BSP`
: A real-board support package. It owns board facts and hardware binding, such
  as clock tree, pinmux, UART route, IRQ controller, memory map, startup, linker
  script inputs, and vendor SDK glue.

`target`
: A build leaf. It selects toolchain, architecture, backend, and optional board
  or profile details. `targets/rk3506` remains a valid target directory.

`platform`
: A historical Charm source area. It may contain reusable prototypes, but new
  backend architecture must not be rooted there.

`HAL`
: A vendor or controller-level interface used below a provider adapter. It is
  not a Charm backend language shared across host, QEMU, and real boards.

`port`
: Board porting or early bring-up entry material. It is reserved for exceptional
  paths such as EarlyConsole and is not a capability boundary term.

## Contract model

Every backend should be explainable as:

```text
BackendIdentity
  + ProviderInstances
  + CapabilityExports
  + EvidenceExports
  + BackendFacts
```

### Backend identity

Backend identity answers what carrier is running Charm. v0 identity should be
expressible without probing concrete devices.

Minimum fields:

- backend kind: `host`, `qemu`, or `board`
- backend name: stable lowercase name, such as `win32`, `linux`, `virt-armv7a`,
  or `rk3506`
- architecture when relevant
- implementation status: `prototype`, `reference`, or `landing`

### Capability export

Capability export answers what provider instances can provide to the Charm
capability composition layer. A profile binding targets a provider instance,
not a provider type, adapter, transport, HAL handle, or endpoint name.

Minimum v0 topology fields:

- capability name
- requirement role
- provider instance
- provider type
- backend identity
- runtime domain
- adapter identity
- binding selection source when available

Host v0 examples:

- monotonic clock
- console text sink
- optional input source
- loopback byte channel
- optional block device backed by a host file

QEMU v0 examples:

- early console
- timer observation
- exception/trap observation
- interrupt controller observation
- memory map facts

Real-board BSP v0 examples:

- early UART route
- clock source
- pinmux route
- IRQ controller
- memory regions
- storage device facts

### Evidence export

Evidence export answers what the backend can prove after bring-up or smoke
execution.

Evidence is structured fact data, not a log format. Logs may present evidence,
but logs are not the contract.

`Backends/contract/backend_evidence.hpp` provides the v0 common evidence
surface for backend identity, capability exports, selected bindings, backend
facts, and readiness summary counters. It intentionally does not define
console-specific TX counters, block geometry, QEMU trap fields, or board probe
payloads.

Minimum v0 evidence:

- backend identity
- exported capability list
- provider instance list
- selected bindings
- provider type metadata
- adapter metadata
- runtime domain identity
- required facts count
- provided facts count
- missing required facts count
- source of each important fact when available

### Backend facts

Facts are used to make backend readiness explicit.

Each fact has:

- kind
- name
- requirement: required or optional
- state: provided, missing, or unknown
- source

The contract should stay compatible with later projection into system compiler
reports, host smoke reports, and board bring-up evidence bundles.

## Dependency rule

Backend contract documents and future contract code must not depend on
`Modules/platform`.

Concrete backend implementations may temporarily wrap existing platform
prototypes during migration, but the dependency direction must be explicit:

```text
Backends/* implementation -> legacy adapter, if needed
Modules/system            -> backend contract or capability registry only
```

New `Modules/system` code must not import concrete host, QEMU, or board backend
implementations.

## Contract candidate smokes

The first backend topology and evidence surface are proven by contract-local
header smokes, host/QEMU/board reference smokes, and host-only system smokes.
The system smokes are prototype-local by design and do not promote their
domain-specific evidence types to `Modules/`. All topology smokes share
`Backends/contract/capability_topology.hpp` for the common topology vocabulary.

- `Backends/contract/topology_header_smoke` proves the header itself can enforce
  provider instance token constraints, binding validity, duplicate binding
  rejection, metadata presence, and `ContextView` access.
- `Backends/contract/evidence_header_smoke` proves backend identity,
  capability export metadata, selected binding evidence, required/provided
  fact counts, missing/unknown required fact counts, and readiness summary.
- `Backends/contract/console_output_header_smoke` proves the first v1
  candidate slice for `ByteSink`, `TextSink`, `LineSource`, console roles,
  transfer/status semantics, and structured console provider evidence.
- `Backends/host/reference_smoke` proves the first host reference backend can
  provide memory-backed console and block providers while exporting a common
  `BackendEvidenceView`.
- `Backends/qemu/reference_smoke` proves the first QEMU reference backend can
  export early console, timer, trap, IRQ, and memory-map evidence without
  pretending to validate H747 external peripherals.
- `Backends/board/reference_smoke` proves the first real-board reference
  evidence surface can export H747 provider identities and board facts while
  keeping missing/unknown required facts from claiming backend readiness.
- `Examples/system/capability_topology_bridge_smoke` proves the generic
  `Requirement -> Binding -> Provider Instance -> Adapter` topology, including
  negative checks that provider types, adapters, backend tokens, HAL tokens, and
  block endpoints cannot be binding targets.
- `Examples/system/console_output_provider_smoke` proves the first
  `console/output` provider candidate using `TextSink.log` and
  `LineSource.shell`, with structured provider evidence kept outside app
  context.
- `Examples/system/block_storage_provider_smoke` proves the first
  `block/storage` provider candidate using `BlockDevice.app_store`, keeping
  provider instance identity distinct from provider-published `BlockEndpoint`.

## Migration order

1. Establish `Backends/` documentation and vocabulary.
2. Treat `Modules/platform` as legacy/prototype until each useful part is
   promoted intentionally.
3. Wrap the existing Windows stub as a future `host.win32` reference backend.
4. Describe QEMU backends through early console, timer, trap, IRQ, and memory
   facts.
5. Let real-board BSPs declare board facts and capability exports without
   redefining the common contract.

## Acceptance checks

- A reader can distinguish backend, BSP, target, and platform.
- A reader can distinguish capability, requirement, binding, provider type,
  provider instance, adapter, HAL, port, endpoint, and evidence.
- Profile binding targets provider instances only.
- Provider type, adapter, transport, HAL handle, backend token, and endpoint name
  are not valid binding targets.
- Contract material under `Backends/contract/` does not import or require
  `Modules/platform`.
- Existing build leaves continue to work while migration is staged.
- Future host backend work can extend the current reference with OS-backed
  clock, console, loopback channel, and file-backed block providers.

## Current gate

Run the current Backends contract candidate gate with:

```powershell
.\Backends\run-backends-v1-smoke.ps1
```
