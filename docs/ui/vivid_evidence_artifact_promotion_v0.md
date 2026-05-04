# Vivid Evidence Artifact Promotion v0

This document defines the promotion boundary for Vivid Evidence Plane vocabulary.

The rule is deliberately conservative:

```text
Keep support implementations demo-side.
Promote stable evidence vocabulary into law.
Promote only runtime-native ledgers into core contracts.
```

`Examples/ui/vivid/support/vivid_evidence_support.hpp` is a lab support file. It may collect repeated helper code, stabilize stdout fields, and host experimental evidence vocabulary. It must not be moved wholesale into Vivid core.

## Why This Exists

Vivid Evidence Lab now has shared vocabulary for:

```text
state truth
  -> invalidation intent
  -> dirty / DrawCmd evidence
  -> render artifact delta
  -> causal verdict
```

This is stronger than local demo printing, but weaker than a core runtime API. v0 records which names are stable enough to become law vocabulary, while keeping their current helper implementation outside the runtime hot path.

## Promotion Tests

An evidence artifact may become a core-facing contract only if it passes all three tests:

```text
1. It describes a runtime semantic fact, not a demo collection detail.
2. It is reused by at least two independent evidence chains.
3. It does not depend on stdout, CTest, DefaultCanvas, or demo input simulation.
```

If an item fails any test, it may still be good Evidence Lab vocabulary. It should remain demo-side until the failing boundary is removed.

## Demo-Only Support

These helpers remain demo-only:

```text
RunLog
expect()
print_* helpers
click_center()
mouse_down_center()
mouse_up_center()
render_scene()
render_component_artifact_delta()
prepare_style_sheet()
hash_dirty()
hash_bytes()
hash_cmd_stats()
FocusMoveTrace
PointerFocusTrace
collect_focus_move()
collect_pointer_focus_trace()
count_click_events_since()
```

Rules:

- Demo support may use stdout and CTest-oriented field assembly.
- Demo support may depend on `DefaultCanvas` and local input simulation.
- Demo support may change as Evidence Lab evolves.
- Demo support must not be treated as a Vivid core API promise.

## Candidate Vocabulary

These names are candidate Evidence Plane vocabulary. Their field semantics may be referenced by law documents before any implementation moves into core:

```text
StateDeltaEvidence
InvalidationEvidence
RenderEvidence
RenderArtifactDeltaEvidence
CausalChainEvidence
```

Current status:

| Artifact | v0 status | Promotion note |
| --- | --- | --- |
| `StateDeltaEvidence` | candidate vocabulary | Field law may stabilize around `id/key/old/new/changed/source`, but collection remains demo-side. |
| `InvalidationEvidence` | candidate vocabulary | Field law may stabilize around `kind/dirty_scope/component_bounds/layout_changed`. |
| `RenderEvidence` | candidate vocabulary | Needs canvas/backend-neutral shape before core promotion. |
| `RenderArtifactDeltaEvidence` | strong candidate | The vocabulary is stable, but current capture still depends on demo render helpers. |
| `CausalChainEvidence` | candidate vocabulary | Keep demo-side until more than Intent-to-Artifact consumes it. |

Candidate vocabulary may appear in docs as law terms. That does not imply the current C++ helper type is a public API.

Field semantics for these candidate artifacts are defined in `vivid_evidence_vocabulary_law_v0.md`.

## Core Runtime Ledgers

These are closer to core because they are derived from runtime decisions or completed runtime execution:

```text
SemanticFocusRequestLedger
SemanticActionRequestLedger
PageTransitionLedger
LayerProfileDecision
LayerAdmission
ResolvedStyleEvidence
StyleStateEvidence
```

Rules:

- Runtime-native ledgers must be derivable from the completed runtime result.
- They should not depend on demo stdout helpers.
- Printing a ledger remains demo-side; the ledger fact may be core.
- Rejection/fallback paths must preserve the boundary or reason that made the decision.

## Do Not Promote

Never promote these categories into Vivid core:

```text
stdout formatting functions
CTest case counters
demo pointer setup
demo click simulation
DefaultCanvas hashing helpers
demo fixture setup
one-off scenario assertions
```

They are useful laboratory tools, not runtime semantics.

## Relationship To Existing Laws

- `vivid_render_evidence_chain_v0.md` defines the state-to-artifact causal chain.
- `vivid_evidence_lab_manifest_v0.md` records CTest-gated Evidence Lab demo coverage and keeps manifest drift visible.
- `vivid_evidence_vocabulary_law_v0.md` defines stable field semantics for candidate evidence vocabulary.
- `vivid_intent_to_artifact_evidence_v0.md` proves one vertical semantic intent-to-artifact chain.
- `vivid_semantic_request_ledger_law_v0.md` defines runtime semantic request ledger law.
- `vivid_evidence_stdout_law.md` governs stdout shape, not core API shape.

This promotion law decides where each evidence item may live as Vivid matures.

## v0 Decision

For now:

```text
vivid_evidence_support.hpp stays demo-side.
Stable fields become Evidence Plane law vocabulary.
Runtime-native ledgers remain the only core-facing evidence artifacts.
```

This keeps Vivid core clean while allowing Evidence Lab language to stabilize.
