# H747 Lab Host-Proof Migration Boundary

> status: `supporting`
>
> scope: what may cross from Host/QEMU prototypes into H747 source, App ABI and tooling

Host smoke proves behavior in one evidence domain. It does not grant its local
types, vocabulary or helpers a place in H747 or Charm Core. Admission remains
subject to [`CONSTITUTION.md`](../../../../CONSTITUTION.md) and the
[`Charm Core contract`](../../../../docs/architecture/charm_core_contract.md).

## Transfer Behavior, Not Prototype Shape

The following may transfer only after source and tests confirm the same
consumer, lifecycle and failure semantics:

- a capability needed by App code without exposing board/HAL identity;
- explicit success, blocked and failure results;
- deterministic load/start/exit diagnostics;
- resource limits that are observable on both the prototype and target;
- evidence labels that keep Host, QEMU and real-board proof distinct.

Do not copy smoke-local builders, reflected types, fixed buffers, presentation
records or helper names merely because a Host test passed.

## Three Boundaries

### Project source boundary

H747 applications and services may use local typed interfaces and compile-time
composition. Board, HAL, cache, IRQ and peripheral ownership stay in project or
backend code. These local types are not automatically public Charm vocabulary.

### Resident App ABI boundary

ELF/ModuleX Apps cross a C-compatible boundary:

```text
AppImage -> loader -> AppRuntime -> charm_app_main(CharmAppApi*, argc, argv)
```

Only explicit ABI data and function tables cross this boundary. C++ templates,
concept identity, name mangling, exceptions, RTTI and reflection tokens do not.
Transport, Store media and runtime-domain identity also remain outside the App
ABI.

### Evidence/tooling boundary

Diagnostics may explain selected inputs, load stages, board facts and failure
reasons. They are read-only observations, not init ordering, provider lookup,
App-visible state or runtime policy. A report name cannot prove that hardware is
present or working.

## Vocabulary Guard

Historical RTE prototypes used names such as `Component`, `Profile`,
`Projection`, `ContextView`, `World` and `Provider`. Those names are not H747 or
Core contracts by inheritance. Reusing one requires a current consumer, stable
cross-environment meaning, explicit failure behavior and independent evidence.
Otherwise keep the mechanism local and name it after its actual role.

## Decision Test

For a candidate structure, ask:

1. Is it App-visible behavior, project/backend implementation, ABI data or
   evidence presentation?
2. Who owns its lifetime and failure recovery?
3. Does it need runtime substitution, compile-time binding or neither?
4. Which Host/QEMU/board evidence proves it, and what remains unproven?
5. Can a smaller local type express the requirement without adding a platform
   noun?

If one type attempts to be App API, board binding, loader policy and evidence
record simultaneously, split the responsibilities before promotion.

Current H747 dynamic-image roles are documented in
[`h747_lab_dynamic_boundary_roadmap.md`](h747_lab_dynamic_boundary_roadmap.md).
Project layering remains local to
[`h747_lab_layering_contract.md`](h747_lab_layering_contract.md).
