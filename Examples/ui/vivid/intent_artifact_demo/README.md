# intent_artifact_demo

This demo is the first Vivid Intent-to-Artifact Evidence v0 sample.

It verifies one semantic product intent against a small SettingsRow-style component:

```text
SemanticActionRequest
  -> SemanticActionRequestLedger
  -> state delta
  -> paint-only invalidation intent
  -> dirty / DrawCmd evidence
  -> render artifact evidence
```

The positive case activates `settings.wifi.toggle` through `request_semantic_action()` and observes the normal checkbox click law toggling `checked=false -> true`.

The negative case disables the target before the request and proves rejected admission does not mutate state or render artifact.

Stdout follows `docs/ui/vivid_evidence_stdout_law.md`:

```text
[ia] run=intent_artifact_demo phase=end result=ok cases=9
```

Build locally:

```powershell
cmake -S Examples/ui/vivid/intent_artifact_demo -B cmake-build-vivid-intent-artifact-demo-codex -G Ninja
cmake --build cmake-build-vivid-intent-artifact-demo-codex
ctest --test-dir cmake-build-vivid-intent-artifact-demo-codex --output-on-failure
```
