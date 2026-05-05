# Vivid Causal Verdict Law v0

This document defines the v0 law for Vivid causal verdicts.

Its purpose is narrow: keep `causal_chain` from becoming only a final "all cases passed" line. A causal verdict must summarize a connected UI runtime cause-and-effect path, and the evidence segments behind that verdict must remain visible in stdout or the paired law document.

## Positioning

Vivid Evidence Plane has three maturity levels:

```text
Evidence Point
  A single observable fact.

Evidence Chain
  Multiple facts connected in runtime order.

Causal Verdict
  A final judgement that the chain committed, rejected, downgraded, or rolled back with explicit evidence.
```

Examples:

```text
Evidence Point:
  snapshot_count=0
  state_delta=1
  artifact_delta=1

Evidence Chain:
  semantic request -> state delta -> invalidation -> render artifact

Causal Verdict:
  causal_chain=1 name=settings.wifi.toggle.activate ok=1 ...
```

`CausalChainEvidence` remains candidate Evidence Plane vocabulary. This law defines verdict eligibility and meaning; it does not promote `Examples/ui/vivid/support/vivid_evidence_support.hpp` into Vivid core.

## AxisCausal Eligibility

`AxisCausal` is not granted just because a demo prints `causal_chain=1`.

A demo may claim `AxisCausal` only when it proves all of these:

```text
1. Intent or request exists.
2. Admission, policy, or precondition is checked.
3. Execution, rejection, fallback, cancel, or rollback has a clear boundary.
4. At least one observable consequence is reported.
5. A final verdict ties the required evidence segments together.
```

Observable consequences may be state, focus, time, transaction, layer, budget, invalidation, DrawCmd, render artifact, snapshot lifetime, or semantic artifact facts.

Rules:

- The verdict must be derived from required evidence segments, not hand-authored as an independent `ok=1`.
- Rejection, no-op, fallback, cancel, or rollback paths count as causal only when they also prove no unintended mutation or no leaked resource.
- A final verdict should be the summary, not the only evidence line.
- A demo should not claim `AxisCausal` when it only verifies static metadata or stdout formatting.

## Verdict Shape

The canonical stdout shape is still governed by `vivid_evidence_stdout_law.md`:

```text
[tag] case=causal_chain causal_chain=1 name=<stable_name> ok=<0|1> ...
```

`vivid_evidence_vocabulary_law_v0.md` defines the shared candidate fields:

```text
request_ok
state_delta_ok
invalidation_ok
artifact_ok
rejected_no_mutation
```

This law adds a verdict expectation:

```text
causal_chain=1
name=<stable chain name>
ok=<derived final verdict>
```

The chain name must be stable. Prefer product-semantic names for semantic chains and runtime-law names for time or transaction chains:

```text
settings.wifi.toggle.activate
motion_time.managed
page_transition.transaction
```

## Count-Based And Evidence-Referenced Verdicts

v0 recognizes two verdict styles.

### Count-Based Verdict

A count-based verdict closes a demo by checking that prior named cases completed:

```text
prior_cases_complete=1
```

This is allowed for existing transitional verdicts when the prior cases are stable and the paired law document names the evidence segments being closed.

Examples:

```text
motion_time.managed
page_transition.transaction
```

### Evidence-Referenced Verdict

An evidence-referenced verdict names the segment verdicts directly in the final line:

```text
request_ok=1
state_delta_ok=1
invalidation_ok=1
artifact_ok=1
rejected_no_mutation=1
```

New causal demos should prefer evidence-referenced verdicts. Existing count-based verdicts do not need immediate churn, but future edits should move them toward evidence-referenced fields when doing so improves clarity without adding noise.

## Verdict Families

### Semantic Verdict

Semantic verdicts explain how product intent or semantic lookup crosses runtime law.

Required evidence normally includes:

```text
resolution or request ledger
admission / rejection status
execution boundary or planning-only boundary
state, focus, event, semantic artifact, or render artifact consequence
rejected-no-mutation evidence when rejection is tested
```

Examples:

```text
semantic action request
semantic focus request
intent-to-artifact
```

### Time Verdict

Time verdicts explain how UI time is owned by runtime rather than by page-local frame loops.

Required evidence normally includes:

```text
managed time source
motion recipe/profile decision
compose or page motion trace
budget/profile/admission effect when relevant
final bounded state after the managed time path
```

`motion_time.managed` is a v0 hybrid verdict. It keeps `cases_closed` as a transitional count-based guard, and also emits `time_ok`, `recipe_ok`, `compose_ok`, `budget_ok`, `trace_ok`, and `page_motion_ok` as evidence-referenced Time-axis fields.

### Transaction Verdict

Transaction verdicts explain how a runtime lifecycle operation commits, cancels, aborts, interrupts, or rolls back.

Required evidence normally includes:

```text
begin/admission boundary
owned artifact acquisition
commit/cancel/interrupt/fallback branch
release/thaw/restore boundary
no leaked snapshot or stale transaction state
```

`page_transition.transaction` is a v0 transitional count-based verdict. It is valid because the page transition demo closes commit, cancel, interrupt, admission, static cut, and snapshot lifecycle evidence before the final verdict.

### Render / State Verdict

Render/state verdicts explain how state truth becomes bounded visual artifact evidence.

Required evidence normally includes:

```text
state delta or no-state-delta proof
invalidation impact
dirty scope or containment
DrawCmd and/or render artifact delta
rejected/no-op artifact stability when tested
```

Examples:

```text
component settings row
component card state
style token law
focus boundary / transfer / scope
```

## Manifest Relationship

`vivid_evidence_lab_manifest_v0.md` owns the demo-to-axis map.

Manifest rows claiming `AxisCausal` must satisfy this law by one of these routes:

```text
1. The demo stdout uses evidence-referenced causal fields.
2. The primary law document explains the evidence segments closed by a count-based verdict.
```

The manifest should keep `intent_artifact_demo` as the vertical causal anchor until a broader cross-axis demo supersedes it.

## Non-Goals

- This law does not add a new demo or case count.
- This law does not define screenshot golden files.
- This law does not define C++ public API.
- This law does not promote demo support helpers into Vivid core.
- This law does not require all existing count-based verdicts to be rewritten immediately.
- This law does not replace `vivid_evidence_vocabulary_law_v0.md`; it consumes that field vocabulary.
