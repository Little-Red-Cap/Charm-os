# Vivid Intent-to-Artifact Evidence v0

This document defines the first vertical Vivid evidence chain that connects semantic intent execution to render artifact consequences.

## Positioning

Earlier Vivid evidence demos prove local laws:

```text
semantic request ledger
component state truth
invalidation intent
dirty / DrawCmd evidence
render artifact hash
```

Intent-to-Artifact Evidence v0 connects those laws into one causal chain:

```text
SemanticActionRequest
  -> SemanticActionRequestLedger
  -> StateDeltaEvidence
  -> InvalidationEvidence
  -> DrawCmdEvidence
  -> RenderArtifactEvidence
```

The goal is not screenshot CI. The goal is to prove why a product semantic request legally produced a visual consequence.

## v0 Sample

`Examples/ui/vivid/intent_artifact_demo` builds a small SettingsRow-style component:

```text
SettingsRow
  id=settings.wifi
  child toggle id=settings.wifi.toggle
```

Positive path:

```text
semantic activate settings.wifi.toggle
  -> action request committed
  -> normal checkbox click law toggles checked false -> true
  -> paint_only invalidation intent
  -> dirty rect remains within SettingsRow bounds
  -> DrawCmd / pixel artifact changes
```

Negative path:

```text
disabled settings.wifi.toggle
  -> action admission rejected
  -> no click execution
  -> no state delta
  -> render artifact remains unchanged
```

## Evidence Rules

- A semantic request must cross the normal runtime request boundary; demo code must not set widget state directly to simulate execution.
- A committed action request must be paired with a named state delta.
- A rejected action request must prove no state delta and no render artifact mutation.
- Invalidation evidence must state the intended impact class, such as `paint_only`.
- Dirty evidence must prove component containment when the evidence claims a component-local mutation.
- Draw command and pixel hashes are evidence summaries, not visual design approvals.

## Stdout Shape

The demo follows `vivid_evidence_stdout_law.md`:

```text
[ia] run=intent_artifact_demo phase=begin
[ia] case=request_ledger ledger=action_request stage=execution status=executed ...
[ia] case=state_delta state_delta=1 id=settings.wifi.toggle key=checked old=0 new=1 changed=1 source=semantic_action_request
[ia] case=rejected_no_state_delta state_delta=0 id=settings.wifi.toggle key=checked old=1 new=1 changed=0 ...
[ia] case=invalidation invalidation=1 kind=paint_only dirty_scope=component layout_changed=0 ...
[ia] case=render_artifact artifact_delta=1 changed=1 dirty_within_component=1 single_dirty_rect=1 after_dirty_count=<n> ...
[ia] case=rejected_artifact artifact_delta=0 changed=0 dirty_within_component=1 single_dirty_rect=1 ...
[ia] case=causal_chain causal_chain=1 name=settings.wifi.toggle.activate ok=1 request_ok=1 state_delta_ok=1 invalidation_ok=1 artifact_ok=1 rejected_no_mutation=1
[ia] run=intent_artifact_demo phase=end result=ok cases=9
```

The final line is the CTest audit gate.

## Non-Goals

- This v0 does not define a generic StateDelta core API.
- `StateDeltaEvidence` currently lives in demo support as evidence vocabulary, not runtime contract surface.
- `InvalidationEvidence` currently lives in demo support as evidence vocabulary, not runtime contract surface.
- `RenderEvidence` printing currently lives in demo support as evidence vocabulary, not runtime contract surface.
- `RenderArtifactDeltaEvidence` currently lives in demo support as evidence vocabulary, not runtime contract surface.
- `CausalChainEvidence` currently lives in demo support as evidence vocabulary, not runtime contract surface.
- This v0 does not introduce screenshot golden files.
- This v0 does not require every component demo to become semantic.
- This v0 does not replace `vivid_semantic_request_ledger_law_v0.md`; it consumes that ledger law.
