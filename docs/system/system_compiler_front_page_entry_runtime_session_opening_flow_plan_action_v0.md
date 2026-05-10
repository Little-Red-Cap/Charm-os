# System Compiler Front Page Entry Runtime Session Opening Flow Plan Action v0

`system_compiler.front_page_entry_runtime_session_opening_flow_plan_action/v0`
is the dedicated bridge from:

- `minimal_kernel.runtime_session_witness_inspect_compare_consumer/v0`

into the front-page opening-flow stack.

It is intentionally narrow.

It does not become a second compare brain.

It does not reopen raw runtime/session/world-compare evidence.

It only projects the already-made reading judgment from the inspect compare
consumer into one explicit opening-flow action.

## Why this bridge exists

The runtime-session inspect compare consumer already answers:

- which drift should be read first
- which explain hop should be opened first
- which artifact is the preferred explanation target

The front-page opening-flow stack should not recompute those answers from raw
evidence again.

This bridge preserves that boundary:

```text
runtime session witness compare
  -> inspect compare consumer
  -> runtime-session opening-flow plan action bridge
  -> runtime-session open-event wrapper
  -> standard open_event
  -> standard open_event_witness
```

The lower seam decides what should be read.

The upper seam only turns that decision into an explainable opening action.

## Relationship to OpeningJudgmentCorridor

Within `OpeningJudgmentCorridor`, this bridge belongs to `Judgment production`.

Its job is to carry the already-exported runtime-session reading judgment into
the standard opening-flow stack.

It must not:

- reopen raw runtime/session/world-compare evidence
- replace `open_event`
- replace witness, landing, route, explain-entry, or handoff layers

## Current shape

Current `system_compiler.front_page_entry_runtime_session_opening_flow_plan_action`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_runtime_session_opening_flow_plan_action.v0.schema.json`
- sample
  - `schemas/examples/system_compiler.front_page_entry_runtime_session_opening_flow_plan_action.v0.sample.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_runtime_session_opening_flow_plan_action.py`
- smoke
  - `scripts/system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_runtime_session_opening_flow_plan_action_bridge_blocked_smoke.ps1`

## What the bridge records

The current summary records:

- `source_consumer`
  - consumer `result`
  - consumer-level compare changed flag
  - default focus id / kind / severity
  - default explain hop id / reason kind
- `judgment_inputs`
  - `consumer_summary_ref`
  - `default_focus_ref`
  - `selected_explain_hop_ref`
  - `selected_artifact_ref`
  - `fallback_artifact_refs`
- `facade_surface`
  - the consumer summary/report/check surface
  - this stays the primary opening facade
- `artifact_target`
  - the selected explain-hop artifact ref
  - fallback artifact refs preserved only as context
- `open_action`
  - fixed `action_id=open-default`
  - fixed `entry_name=runtime-session-inspect-consumer`
  - fixed `expected_consumer_operation=open-consumer-summary`
  - fixed `projection_kind=kernel_runtime_session_opening_judgment`
- `opening_preview`
  - headline / summary lines / question lines copied from `default_focus`
- `execution_receipt`
  - `chosen_by=default_focus/default_explain_hop`
  - `planned_action_count=1`

## Ready / blocked policy

This bridge uses a deliberately fixed readiness rule.

`open_action.status=ready` only when:

- source consumer `result=ok`
- `default_focus` exists
- `default_explain_hop.artifact_ref.path` exists
- `consumer_summary_ref` exists

Otherwise:

- `open_action.status=blocked`
- violations only explain the missing bridge refs

This layer does not inspect raw runtime/session evidence to invent more
blockers.

## Runtime-session open-event wrapper

The bridge is not consumed by the generic open-event exporter directly.

Instead, a runtime-session-specific wrapper produces a standard
`system_compiler.front_page_entry_opening_flow_open_event/v0`:

- exporter
  - `scripts/export_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py`
- end-to-end smoke
  - `scripts/system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1`

That wrapper keeps the contract:

- facade is primary
- artifact is a selected target, not a new policy engine
- fallback explain hops do not become rejected consumers

## Why this matters

This bridge is the first lawful projection from:

- session witness inspect judgment

to:

- front-page opening judgment

without duplicating compare semantics.

That makes the runtime-session path explainable from the first session-focused
consumer summary all the way to `open_event_witness`, while keeping the
existing generic opening-flow stack intact.
