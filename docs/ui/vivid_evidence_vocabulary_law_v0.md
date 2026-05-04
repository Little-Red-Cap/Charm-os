# Vivid Evidence Vocabulary Law v0

This document defines the v0 field vocabulary for candidate Vivid Evidence Plane artifacts.

It does not promote `Examples/ui/vivid/support/vivid_evidence_support.hpp` into core. It only makes the stable field meanings explicit so demos, docs, and future tooling can use the same language.

## Scope

v0 covers these candidate vocabulary artifacts:

```text
StateDeltaEvidence
InvalidationEvidence
RenderEvidence
RenderArtifactDeltaEvidence
CausalChainEvidence
```

These names describe evidence language, not public runtime API. Promotion rules are defined by `vivid_evidence_artifact_promotion_v0.md`.

## General Rules

- Evidence fields describe observed or declared UI causality.
- Evidence fields must be deterministic and grep-friendly.
- Field names should remain stable once they appear in stdout law or CTest-gated demos.
- Field values must avoid pointers, addresses, elapsed wall time, random ids, and localized text.
- A helper implementation may change while preserving the field law.

## StateDeltaEvidence

State delta evidence answers:

```text
Which state truth changed?
What was the old value?
What is the new value?
Who caused the change?
```

Canonical fields:

| Field | Meaning |
| --- | --- |
| `state_delta` | `1` when `old != new`, otherwise `0`. |
| `id` | Stable product/runtime id for the state owner. |
| `key` | Stable state key such as `checked`, `value`, `text`, `focused`, or `visible`. |
| `old` | Integer or stable enum value before the change. |
| `new` | Integer or stable enum value after the change. |
| `changed` | Duplicate boolean verdict for grep-friendly stdout; must equal `state_delta`. |
| `source` | Stable source name such as `user_input`, `programmatic`, `semantic_action_request`, `motion`, or `page_transaction`. |
| `reason` | Optional stable reason for no-op or rejection paths. |

Rules:

- `changed` must be derived from `old != new`.
- A rejected request that does not mutate state should still be allowed to emit `state_delta=0` when proving no mutation.
- Multi-delta cases may prefix fields, but each delta must preserve the same vocabulary.
- A state delta alone does not imply layout or repaint; invalidation evidence must state that separately.

## InvalidationEvidence

Invalidation evidence answers:

```text
What impact did the state/style/content change claim?
Where is the claimed dirty scope?
Did the change require layout?
```

Canonical fields:

| Field | Meaning |
| --- | --- |
| `invalidation` | `1` when an invalidation claim is present. |
| `kind` | Stable impact class. |
| `dirty_scope` | Claimed dirty ownership scope. |
| `component_x/y/w/h` | Component bounds used for containment evidence. |
| `layout_changed` | `1` when the change requires layout, otherwise `0`. |

Allowed `kind` values:

```text
none
paint_only
layout
text_metrics
style
render_cache
```

Allowed `dirty_scope` values:

```text
none
widget
component
page
layer
full_frame
```

Rules:

- `paint_only` must not claim `layout_changed=1`.
- `layout` and `text_metrics` may imply paint, but should still name the layout/text reason.
- If `dirty_scope=component`, render artifact evidence should prove dirty containment when possible.
- Invalidation evidence may be declarative in v0; future runtime integration may produce it directly.

## RenderEvidence

Render evidence summarizes dirty, DrawCmd, execution, and pixel artifact facts after a render pass.

Canonical prefixed fields:

| Field | Meaning |
| --- | --- |
| `<prefix>_dirty_count` | Number of dirty rects recorded for the pass. |
| `<prefix>_dirty_hash` | Stable hash of dirty rect structure. |
| `<prefix>_cmd_count` | Number of recorded draw commands. |
| `<prefix>_cmd_bytes` | Bytes used by the command buffer. |
| `<prefix>_exec_cmds` | Number of executed commands. |
| `<prefix>_failed` | Number of failed commands. |
| `<prefix>_cmd_hash` | Stable summary hash of command/execute stats. |
| `<prefix>_pixel_hash` | Stable pixel artifact hash for the tested backend. |

Rules:

- `failed` must be `0` for a passing visual evidence case unless the case explicitly tests failure.
- `cmd_hash` is draw intent evidence, not a screenshot substitute.
- `pixel_hash` is backend artifact evidence, not a product visual approval by itself.
- Current demo support may depend on `DefaultCanvas`; any future core form must be backend-neutral.

## RenderArtifactDeltaEvidence

Render artifact delta evidence compares two render evidence snapshots.

Canonical fields:

| Field | Meaning |
| --- | --- |
| `artifact_delta` | `1` when a delta verdict is present. |
| `changed` | `1` when render evidence differs from baseline. |
| `dirty_within_component` | `1` when all dirty rects remain inside the claimed component bounds. |
| `single_dirty_rect` | `1` when the artifact pass produced exactly one dirty rect. |

Rules:

- Positive mutation paths usually require `changed=1`.
- Rejected/no-op paths usually require `changed=0`.
- If a case claims component-local mutation, `dirty_within_component` must be `1`.
- `single_dirty_rect=1` is a stronger locality property, not always required by the vocabulary itself.

## CausalChainEvidence

Causal chain evidence is a final verdict tying multiple evidence segments together.

Canonical fields:

| Field | Meaning |
| --- | --- |
| `causal_chain` | `1` when a chain verdict is present. |
| `name` | Stable chain name, often a semantic id plus action. |
| `ok` | Overall positive chain verdict. |
| `request_ok` | Request/admission/ledger segment passed. |
| `state_delta_ok` | State delta segment passed. |
| `invalidation_ok` | Invalidation segment passed. |
| `artifact_ok` | Render artifact segment passed. |
| `rejected_no_mutation` | Rejected path preserved state/artifact when tested. |

Rules:

- `ok` must be derived from the required segment verdicts for the case.
- A chain may include `rejected_no_mutation` as a separate guard; it need not be part of `ok` unless the case says so.
- `name` must be stable and product-semantic when possible.
- Causal chain evidence should be the summary, not the only evidence emitted.

## Relationship To Stdout

`vivid_evidence_stdout_law.md` governs line shape:

```text
[tag] case=<case> key=value ...
```

This document governs the meaning of the `key=value` vocabulary for candidate artifacts. Printing remains demo-side.

## Non-Goals

- This law does not define C++ public API.
- This law does not require all widgets to emit state delta objects.
- This law does not define screenshot golden files.
- This law does not replace runtime-native ledger contracts such as `SemanticActionRequestLedger`.
- This law does not promote demo support helpers into Vivid core.
