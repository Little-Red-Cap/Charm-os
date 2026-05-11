# Vivid Semantic Action State Transition Law v0

This document defines the v0 law for a composite semantic-action sample that carries semantic intent through state/render evidence and then into a page transaction.

Its purpose is narrow: `semantic_action_state_transition_demo` proves a wider cross-axis assembly than `semantic_transition_demo`. The demo is legal only when the semantic request first proves a normal execution path, then proves state/render consequence, and only then begins a page transition. The demo must not let semantic code directly own page truth, and it must not collapse state/render evidence into transaction admission.

`Examples/ui/vivid/semantic_action_state_transition_demo` is the current conformance sample for this law. Its sample details remain documented in `vivid_semantic_action_state_transition_evidence_v0.md`.

## Legal Chain

The v0 legal chain is:

```text
Semantic Action Request
  -> Semantic Admission
  -> Edge Evidence
  -> State Delta Evidence
  -> Invalidation Evidence
  -> Render Artifact Evidence
  -> Transaction Admission
  -> PageTransitionRunner
  -> Layer Admission
  -> Causal Verdict
```

The chain proves that product intent crosses Vivid runtime subsystems through explicit boundaries:

```text
semantic request
  -> emitted runtime edge
  -> state delta
  -> invalidation
  -> render artifact delta
  -> application-side bridge
  -> page transaction begin/sample/commit or rejection
  -> layer snapshot lifecycle
  -> causal_chain verdict
```

The application-side bridge may start a transaction only after request ledger evidence proves execution and the state/render bridge evidence proves the component mutation. In v0 this means `Executed`, `emitted_click=1`, and visible state/render evidence.

## Boundary Laws

### Semantic Intent Does Not Own Page Truth

A semantic request must not directly mutate page truth.

Forbidden shortcut:

```text
semantic request -> set current page
```

Legal path:

```text
semantic request
  -> admitted edge
  -> state/render bridge
  -> PageTransitionRunner
  -> commit
  -> page truth changes
```

This keeps Semantic axis evidence from bypassing Transaction axis evidence.

### Semantic Admission Is Not Transaction Admission

Semantic admission only proves the semantic target and requested action are valid enough to execute or reject under semantic law.

It does not prove:

```text
transition resources are available
LayerAdmission permits capture
PageTransitionRunner can begin
backend/profile/budget permits the requested transition path
```

Cross-axis samples must keep these verdicts separate:

```text
SemanticActionAdmission
StateRenderAdmission
PageTransitionAdmission
LayerAdmission
```

### Edge Evidence Is The Bridge

The edge between semantic request and page transaction must be visible as runtime evidence, such as an emitted click/action event.

The edge is a bridge, not a business shortcut:

```text
Semantic intent: settings.library.open
Edge evidence: activate/click emitted by normal runtime input law
State/render evidence: checked truth changes, invalidation stays bounded, render artifact delta remains local
Transaction: source page -> destination page migration
```

The application-side bridge may start a transaction only after request ledger evidence proves execution and the component bridge evidence is visible.

### State And Render Evidence Must Precede Transaction Begin

This law extends the first semantic-to-transaction sample by requiring a visible state/render consequence before page transaction begin.

The semantic bridge must prove:

```text
checked truth changes
invalidation is paint-only or otherwise bounded
render artifact delta is visible
```

Only after those facts are visible may `PageTransitionRunner` begin the page migration.

### Transaction Owns Commit And Abort

Once a page transaction begins, commit, cancel, abort, interrupt, fallback, and static-cut cleanup are owned by the Transaction axis.

Semantic code must not clean up transaction-owned snapshots, thaw pages, or patch page truth independently.

## Positive Path Requirements

A positive semantic-action state transition must prove:

```text
semantic request executed
edge emitted
state delta observed
invalidation observed
render artifact delta observed
transaction begin admitted
layer admission path observed
transition sample/render evidence observed
commit completed
destination page truth visible
source page truth no longer active/visible as expected
runner returned to idle
snapshot lifecycle closed
```

The final `causal_chain` should be evidence-referenced. For `semantic_action_state_transition_demo`, the verdict fields are:

```text
request_ok
event_ok
state_delta_ok
invalidation_ok
artifact_ok
admission_ok
commit_ok
snapshot_lifecycle_ok
page_truth_ok
rejected_no_mutation
```

## Rejection Path Requirements

A rejected semantic-action state transition must prove no unintended transaction consequence:

```text
semantic request rejected or not executed
no edge emitted
no state delta
no transition begin
page truth unchanged
snapshot_count unchanged
no owned snapshot leaked
render artifact unchanged or otherwise proven stable
```

Rejected paths count as causal evidence only when they prove no mutation or no leaked resource, as required by `vivid_causal_verdict_law_v0.md`.

## Relationship To Evidence Documents

`vivid_semantic_action_state_transition_evidence_v0.md` documents the current sample and stdout shape.

`vivid_semantic_transition_law_v0.md` defines the narrower semantic-to-transaction bridge where state/render evidence is not part of the bridge boundary.

`vivid_intent_to_artifact_evidence_v0.md` defines the vertical intent-to-artifact bridge that this law reuses for state/render evidence.

`vivid_causal_verdict_law_v0.md` defines `AxisCausal` eligibility and count-based versus evidence-referenced verdict rules.

`vivid_evidence_lab_manifest_v0.md` records the demo-to-axis map. Manifest rows that claim semantic-action-state-transition coverage should use this law as the primary boundary document.

## Non-Goals

- This law does not add navigation, callback, or transaction bridge core API.
- This law does not make `SemanticActionRequest` own page transitions.
- This law does not promote demo support helpers into Vivid core.
- This law does not require screenshot golden files.
- This law does not replace `vivid_semantic_transition_law_v0.md`.
- This law does not replace `vivid_intent_to_artifact_evidence_v0.md`.
