# Vivid Semantic Action State Transition Evidence v0

This document records the concrete sample for `semantic_action_state_transition_demo`.

The sample proves a broader cross-axis assembly than `semantic_transition_demo`: semantic action request execution first yields state/render evidence, and only then does the application-side bridge start `PageTransitionRunner` for a page migration.

## Sample Contract

- The source page starts visible and the destination page starts hidden.
- The source page contains one semantic-activate control with the stable semantic id `settings.library.open`.
- The semantic request must first be observed as `Executed` with `emitted_click=1`.
- The state/render bridge must prove checked truth change, bounded invalidation, and changed render artifact.
- The application bridge may start `PageTransitionRunner` only after the request and state/render bridge evidence are visible.
- The positive path must prove page transaction commit and snapshot release.
- The negative path must prove rejected-no-mutation: no transition begin, no page truth change, no snapshot acquisition, and no render mutation.
- The final `causal_chain` must use evidence-referenced fields for semantic, edge, state, invalidation, render, admission, transaction, layer, and page truth segments.

## Stdout Shape

```text
[sastx] run=semantic_action_state_transition_demo phase=begin
[sastx] case=baseline_page_truth source_visible=1 destination_visible=0 snapshots=0 checked=0 ...
[sastx] case=semantic_request_ledger ledger=action_request stage=execution status=executed ...
[sastx] case=state_render_bridge state_delta=1 invalidation=1 artifact_delta=1 ...
[sastx] case=request_event_trace emitted_click=1 click=1 bridge_ready=1 ...
[sastx] case=transition_begin bridge_started=1 status=started admission=pixel_double snapshots=2 ...
[sastx] case=transition_sample valid=1 source_valid=1 destination_valid=1 ...
[sastx] case=transition_commit commits=1 aborts=0 source_visible=0 destination_visible=1 committed=1
[sastx] case=snapshot_lifecycle snapshots=0 released=1 source_caps=1 destination_caps=1 ...
[sastx] case=rejected_request_no_transition ledger=action_request status=rejected ... bridge_started=0 snapshots=0
[sastx] case=causal_chain causal_chain=1 name=settings.library.open.state_transition ok=1 request_ok=1 event_ok=1 state_delta_ok=1 invalidation_ok=1 artifact_ok=1 admission_ok=1 commit_ok=1 snapshot_lifecycle_ok=1 page_truth_ok=1 rejected_no_mutation=1
[sastx] run=semantic_action_state_transition_demo phase=end result=ok cases=10
```

## Relationship To Other Laws

- `vivid_semantic_transition_law_v0.md` defines the narrower semantic-to-transaction bridge.
- `vivid_intent_to_artifact_evidence_v0.md` defines the vertical intent-to-artifact bridge reused for state/render evidence.
- `vivid_causal_verdict_law_v0.md` defines the `AxisCausal` verdict contract and evidence-referenced field rules.

## Non-Goals

- This v0 does not add navigation or callback core API.
- This v0 does not make `SemanticActionRequest` own page transitions.
- This v0 does not replace `semantic_transition_demo`.
- This v0 does not require screenshot golden files.
