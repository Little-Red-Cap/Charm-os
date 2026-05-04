# Vivid Semantic Request Ledger Law v0

This document defines the shared request ledger language for Vivid semantic runtime evidence.
It extracts the common law behind `SemanticFocusRequestLedger` and `SemanticActionRequestLedger`.

## Scope

v0 covers two runtime ledger artifacts:

```text
SemanticFocusRequestLedger
SemanticActionRequestLedger
```

Both artifacts turn a completed request result into a stable Evidence Plane record. Demos may print it, CI may grep it, and future tooling may hash it.

## Common Rules

- A request ledger is an artifact, not a side effect.
- A request ledger must be derivable from the completed request result.
- A request ledger must name the final boundary reached through `stage`.
- A rejected request must not be represented only as `rejected`; the ledger must preserve the failed boundary through `stage` and/or a reason field.
- Stdout evidence should use `ledger=<kind> stage=<stage> status=<status> ...`.
- Demo code should print runtime ledger artifacts instead of reimplementing stage inference locally.
- `Examples/ui/vivid/support/vivid_evidence_support.hpp` owns the demo-side ledger print helpers, so demos do not duplicate ledger field assembly.

## Focus Request Ledger

Focus request chain:

```text
SemanticFocusQuery
  -> SemanticFocusAdmission
  -> SemanticFocusRequest
  -> SemanticFocusRequestLedger
```

| Request result | Ledger stage | Required evidence |
| --- | --- | --- |
| admission rejected | `focus_admission` | `status=rejected`, `admission=<reason>`, `admitted=0` |
| already focused | `already_focused` | `status=already_focused`, `transfer_needed=0`, `focus_out=0`, `focus_in=0` |
| committed transfer | `execution` | `status=committed`, `committed=1`, `focus_out=1`, `focus_in=1` |
| execution failed after admission | `execution` | `status=rejected`, `admitted=1`, `committed=0` |

Canonical stdout shape:

```text
[sfr] case=<case> ledger=focus_request stage=<stage> status=<status> admission=<status> query=<status> ...
```

## Action Request Ledger

Action request chain:

```text
SemanticIntentResolution
  -> SemanticActionAdmission
  -> SemanticFocusRequest
  -> semantic click execution
  -> SemanticActionRequestLedger
```

| Request result | Ledger stage | Reject reason | Required evidence |
| --- | --- | --- | --- |
| action admission rejected | `action_admission` | `action_admission_rejected` | `admitted=0`, `click=0` |
| focus preparation rejected | `focus_request` | `focus_request_rejected` | `admitted=1`, `focus_ready=0`, `click=0` |
| click execution succeeded | `execution` | `none` | `executed=1`, `click=1` |
| execution failed after focus preparation | `execution` | `input_action_overflow` / `no_action_emitted` | `admitted=1`, `focus_ready=1`, `executed=0` |

Canonical stdout shape:

```text
[sar] case=<case> ledger=action_request stage=<stage> status=<status> reason=<reason> intent=<status> ...
```

## Evidence Demos

- `Examples/ui/vivid/semantic_focus_request_demo` proves `SemanticFocusRequestLedger`.
- `Examples/ui/vivid/semantic_action_request_demo` proves `SemanticActionRequestLedger`.
- `Examples/ui/vivid/semantic_focus_query_demo` prints `ledger=focus_query` through the shared support helper.
- `Examples/ui/vivid/semantic_focus_admission_demo` prints `ledger=focus_admission` through the shared support helper.
- `Examples/ui/vivid/semantic_action_admission_demo` prints `ledger=action_admission` through the shared support helper.
- `Examples/ui/vivid/semantic_intent_demo` prints `intent_resolution=1` through the shared support helper.

Current stdout contracts remain owned by `vivid_evidence_stdout_law.md`:

```text
[sfr] run=semantic_focus_request_demo phase=end result=ok cases=7
[sar] run=semantic_action_request_demo phase=end result=ok cases=6
```

`Examples/ui/vivid/intent_artifact_demo` also consumes `SemanticActionRequestLedger` through the shared helper as part of its causal chain evidence.

## Support-Layer Vocabulary

`Examples/ui/vivid/support/vivid_evidence_support.hpp` also owns shared stdout helpers for semantic query, intent resolution, and admission evidence:

```text
intent_resolution=1 ...
ledger=focus_query ...
ledger=focus_admission ...
ledger=action_admission ...
ledger=focus_request ...
ledger=action_request ...
```

Only `SemanticFocusRequestLedger` and `SemanticActionRequestLedger` are runtime ledger artifacts in this v0 law. Query, intent resolution, and admission helpers are demo-lab evidence vocabulary: they keep stdout stable and remove duplicated field assembly from demos, but they do not promote those intermediate results into a new core contract surface.

Promotion boundaries for demo-only helpers, candidate vocabulary, and core-facing runtime ledgers are tracked in `vivid_evidence_artifact_promotion_v0.md`.

## Non-Goals

- This law does not define a general transaction framework.
- This law does not merge focus and action request types.
- This law does not add accessibility execution.
- This law does not require every semantic query or admission to become a runtime ledger artifact.
