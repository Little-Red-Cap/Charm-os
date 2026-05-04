# Kernel Runtime Session Witness v0

## Purpose

This document names the next minimal-kernel evidence object:

```text
kernel_runtime_session
```

The session is not a QEMU report, a host verifier report, or a bigger runtime
log. It is the shared object that upper-half host semantics, lower-half
ARMv7-A QEMU machine execution, the runtime evidence bundle, the system
compiler witness bundle, and world compare all point at.

## Stage Decision

Current stage:

```text
ARMv7-A Lower-Half Evidence Closure
```

This stage answers whether exception, timer, trap, syscall, thread, handoff,
and live runtime evidence stand on the real ARMv7-A QEMU machine path.

Next stage:

```text
Kernel Runtime Session Witness v0
```

This stage answers whether one `minimal_kernel_runtime` session can be proven
as standing by both host-side semantic facts and machine-side ingress facts,
then exported through the existing evidence and witness pipeline.

## Object Boundary

`kernel_runtime_session` is the proved session object.

It is:

- a minimal-kernel runtime session identity
- a host/QEMU shared evidence target
- a compact summary of semantic witness, machine witness, runtime facts,
  ledger references, and session verdict
- a witness-bundle entry source that world compare can reason about

It is not:

- a replacement for QEMU smoke logs
- a replacement for host verifier summaries
- a direct input to world compare
- a promise of full user/kernel isolation, POSIX, VFS, or a complete OS shape

## Evidence Flow

The intended flow is:

```text
host verifier
  -> semantic session facts

ARMv7-A QEMU lower-half smoke
  -> machine session facts

runtime evidence bundle
  -> session/kernel_runtime_session.summary.json

system compiler witness bundle
  -> kernel_runtime_session witness entry

world compare
  -> standing / improved / drifted / collapsed
```

World compare stays at the witness-bundle layer. It should not consume raw
QEMU logs or ad hoc runtime summaries directly.

## Minimum Shape

The v0 session summary is intentionally small. It answers:

- which world and subject this session belongs to
- which host semantic facts are present
- which machine ingress facts are present
- which runtime facts are present
- where the phase/runtime ledgers live
- whether the session is standing
- which failure, if any, explains the session verdict

The schema lives at:

- `schemas/minimal_kernel.kernel_runtime_session.v0.schema.json`

The sample lives at:

- `schemas/examples/minimal_kernel.kernel_runtime_session.v0.sample.json`

## Ledger Boundary

`phase_ledger` and `runtime_ledger` remain different objects.

`phase_ledger` proves broad boot and runtime phase order:

```text
boot -> mmu -> exception -> timer -> trap -> runtime -> handoff
```

`runtime_ledger` proves session events:

```text
tick observed
trap decoded
syscall dispatched
task resumed
thread switched
handoff continuity preserved
```

The session summary defines which facts are required. A log parser may collect
some of those facts, but it does not define what the session is.

## Ingress Vocabulary

v0 freezes ingress meaning before freezing C++ type names:

- `ExceptionIngress`: a machine exception enters the kernel observation domain
- `InterruptIngress`: an external or software interrupt enters the scheduling domain
- `TimerIngress`: a time source enters tick/runtime handling
- `TrapIngress`: a synchronous service request enters kernel service dispatch
- `ContextIngress`: thread context is prepared, saved, restored, or observed
- `RuntimeLoopIngress`: lower-half events are consumed by the runtime loop

Later C++ names may become `ArchExceptionPort`, `ArchInterruptPort`,
`ArchTimerPort`, `ArchContextPort`, `RuntimeLoopPort`, or a more precise local
binding name. The v0 commitment is the evidence vocabulary, not the final type
surface.

## Failure Taxonomy

Session failures should be exported as witness facts, not just script failures.
The v0 codes are:

```text
decode_failed
unsupported_service
unbound_adapter
unbound_bridge
writeback_failed
missing_phase
unexpected_phase
timeout
spurious_interrupt
trap_not_observed
tick_not_observed
thread_not_resumed
handoff_not_landed
handoff_continuity_broken
host_semantic_mismatch
machine_witness_missing
world_contract_missing
```

Each failure should carry at least:

- `code`
- `domain`
- `layer`
- `focus`
- `required`
- `phase`
- `message`

## Handoff Continuity

Handoff belongs to session continuity for v0.

It should not become a separate canonical world until it starts committing to
larger boot-chain responsibilities such as image format, payload verification,
slot/rollback, storage-backed launch, or a next-stage independent world.

For now, handoff answers one session question:

```text
Can the same minimal_kernel_runtime session remain explainable across launch
and landing?
```
