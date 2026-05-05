# Vivid Semantic-to-Transaction Evidence v0

This document defines the first cross-axis Vivid evidence sample that connects semantic request execution to page transition transaction evidence.

## Positioning

Earlier demos prove local axes:

```text
SemanticActionRequest
PageTransitionRunner
Layer admission / snapshot lifecycle
Causal verdict
```

Semantic-to-Transaction Evidence v0 connects them into one chain:

```text
SemanticActionRequest(nav.library.open)
  -> emitted click evidence
  -> demo-side application bridge
  -> PageTransitionRunner begin/sample/commit
  -> page truth and snapshot lifecycle verdict
```

The semantic request does not directly change page truth. The transition may start only after the request ledger reports `Executed` and `emitted_click=1`.

## v0 Sample

`Examples/ui/vivid/semantic_transition_demo` builds a small two-page scene:

```text
source page
  action id=nav.library.open

destination page
  prepared by PageTransitionRunner
```

Positive path:

```text
semantic activate nav.library.open
  -> request ledger executed
  -> one click event is visible after events_before
  -> demo bridge starts PageTransitionRunner
  -> PixelDouble admission captures source and destination snapshots
  -> sample composes both pages
  -> commit makes destination visible and source hidden
  -> snapshot_count returns to 0
```

Negative path:

```text
disabled nav.library.open
  -> action admission rejected
  -> no click emitted
  -> bridge does not start transition
  -> page truth and snapshot_count remain unchanged
```

## Evidence Rules

- The semantic request must use `Scene::request_semantic_action`; demo code must not mutate page truth as a shortcut.
- The bridge to `PageTransitionRunner::begin()` is application-side and must be gated by request execution evidence.
- The positive path must prove page transaction commit and snapshot release.
- The negative path must prove rejected-no-mutation: no transition begin, no page truth change, no snapshot acquisition.
- The final `causal_chain` must use evidence-referenced fields for semantic, edge, admission, transaction, layer, and page truth segments.

## Stdout Shape

The demo follows `vivid_evidence_stdout_law.md`:

```text
[stx] run=semantic_transition_demo phase=begin
[stx] case=baseline_page_truth source_visible=1 destination_visible=0 snapshots=0
[stx] case=semantic_request_ledger ledger=action_request stage=execution status=executed ...
[stx] case=request_event_trace emitted_click=1 click=1 bridge_ready=1 ...
[stx] case=transition_begin bridge_started=1 status=started admission=pixel_double snapshots=2 ...
[stx] case=transition_sample valid=1 source_valid=1 destination_valid=1 ...
[stx] case=transition_commit commits=1 aborts=0 source_visible=0 destination_visible=1 committed=1
[stx] case=snapshot_lifecycle snapshots=0 released=1 source_caps=1 destination_caps=1 ...
[stx] case=rejected_request_no_transition ledger=action_request status=rejected ... bridge_started=0 snapshots=0
[stx] case=causal_chain causal_chain=1 name=nav.library.open.transaction ok=1 request_ok=1 event_ok=1 admission_ok=1 commit_ok=1 snapshot_lifecycle_ok=1 page_truth_ok=1 rejected_no_mutation=1
[stx] run=semantic_transition_demo phase=end result=ok cases=9
```

## Non-Goals

- This v0 does not introduce navigation or callback core API.
- This v0 does not make `SemanticActionRequest` own page transitions.
- This v0 does not replace `vivid_intent_to_artifact_evidence_v0.md`.
- This v0 does not add screenshot golden files.
- This v0 does not promote demo support helpers into Vivid core.
