# Vivid Semantic Transition Law v0

This document defines the v0 law for semantic intent triggering a page transaction in Vivid.

Its purpose is narrow: `semantic_transition_demo` proves the first cross-axis assembly, but this law defines which boundaries make that assembly legal. A semantic request may open the path toward navigation, but it must not bypass runtime transaction, layer admission, or causal verdict laws.

`Examples/ui/vivid/semantic_transition_demo` is the current conformance sample for this law. Its sample details remain documented in `vivid_semantic_transition_evidence_v0.md`.

## Legal Chain

The v0 legal chain is:

```text
Semantic Intent
  -> Semantic Resolution
  -> Semantic Admission
  -> Edge Evidence
  -> Transaction Admission
  -> PageTransitionRunner
  -> Layer Admission
  -> Render Evidence
  -> Causal Verdict
```

The chain proves that product intent crosses Vivid runtime subsystems through explicit boundaries:

```text
semantic request
  -> emitted runtime edge
  -> application-side bridge
  -> page transaction begin/sample/commit or rejection
  -> layer snapshot lifecycle
  -> render/page truth consequence
  -> causal_chain verdict
```

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
PageTransitionAdmission
LayerAdmission
```

### Edge Evidence Is The Bridge

The edge between semantic request and page transaction must be visible as runtime evidence, such as an emitted click/action event.

The edge is a bridge, not a business shortcut:

```text
Semantic intent: nav.library.open
Edge evidence: activate/click emitted by normal runtime input law
Transaction: source page -> destination page migration
```

The application-side bridge may start a transaction only after request ledger evidence proves execution and edge emission. In v0 this means `Executed` and `emitted_click=1`.

### Transaction Owns Commit And Abort

Once a page transaction begins, commit, cancel, abort, interrupt, fallback, and static-cut cleanup are owned by the Transaction axis.

Semantic code must not clean up transaction-owned snapshots, thaw pages, or patch page truth independently.

## Positive Path Requirements

A positive semantic transition must prove:

```text
semantic request executed
edge emitted
transaction begin admitted
layer admission path observed
transition sample/render evidence observed
commit completed
destination page truth visible
source page truth no longer active/visible as expected
runner returned to idle
snapshot lifecycle closed
```

The final `causal_chain` should be evidence-referenced. For `semantic_transition_demo`, the verdict fields are:

```text
request_ok
event_ok
admission_ok
commit_ok
snapshot_lifecycle_ok
page_truth_ok
rejected_no_mutation
```

## Rejection Path Requirements

A rejected semantic transition must prove no unintended transaction consequence:

```text
semantic request rejected or not executed
no edge emitted
application bridge did not start transition
page truth unchanged
snapshot_count unchanged
no owned snapshot leaked
```

Rejected paths count as causal evidence only when they prove no mutation or no leaked resource, as required by `vivid_causal_verdict_law_v0.md`.

## Relationship To Evidence Documents

`vivid_semantic_transition_evidence_v0.md` documents the current sample and stdout shape.

`vivid_causal_verdict_law_v0.md` defines `AxisCausal` eligibility and count-based versus evidence-referenced verdict rules.

`vivid_evidence_lab_manifest_v0.md` records the demo-to-axis mapping. Manifest rows that claim semantic-to-transaction coverage should use this law as the primary boundary document.

## Non-Goals

- This law does not add navigation, callback, or transaction bridge core API.
- This law does not make `SemanticActionRequest` own page transitions.
- This law does not promote demo support helpers into Vivid core.
- This law does not require screenshot golden files.
- This law does not define composite intent with both state/render and page transaction consequences.
