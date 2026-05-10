# Vivid Evidence Lab Manifest v0

This document records the v0 manifest for CTest-gated Vivid Evidence Lab demos.

The manifest is not a screenshot runner, a build orchestrator, or a Vivid core API. It is a small evidence map that keeps demo names, stdout tags, case counts, and coverage axes visible as the lab grows.

## Why This Exists

Vivid Evidence Lab now has many independent proofs:

```text
edge
state
style
render
semantic
focus
motion
transaction
layer
vocabulary
causal chain
admission
```

Without a manifest, the lab can drift into a museum of demos. The manifest keeps the demos arranged as an evidence lattice:

```text
demo -> stdout gate -> evidence axes -> contract document
```

## v0 Runtime Sample

`Examples/ui/vivid/evidence_lab_manifest_demo` is the smallest manifest conformance sample.

It verifies:

```text
registered demos have stable run/tag/cases fields
run names and tags are unique
the total registered case count is explicit
all v0 evidence axes have at least one gated sample
intent_artifact_demo remains the vertical causal anchor
semantic_transition_demo remains the first semantic-to-transaction cross-axis sample with a primary boundary law
evidence_vocabulary_demo remains the field-law anchor
promotion boundaries stay demo-side / law / runtime-ledger separated
stdout law registry matches manifest gates
demo CMake PASS gates match manifest gates
primary law documents point back to their demos
AxisCausal entries remain tied to causal verdict law or primary-doc causal evidence
```

CTest guards the final line:

```text
[elm] run=evidence_lab_manifest_demo phase=end result=ok cases=10
```

The fast smoke entry is:

```powershell
./scripts/vivid_evidence_lab_manifest_smoke.ps1
```

## Manifest Fields

Each manifest row has:

| Field | Meaning |
| --- | --- |
| `run` | Stable demo run name used in stdout. |
| `tag` | Short stdout domain tag. |
| `cases` | Expected final `cases=<n>` value. |
| `axes` | Evidence axes covered by the demo. |
| `primary_doc` | First law or route document that owns the demo's evidence meaning. |

The manifest row must match `vivid_evidence_stdout_law.md` when a demo is CTest-gated by that law.
The primary document must mention the demo path so route drift is visible.

## Coverage Axes

v0 uses these axes:

```text
edge
state
style
render
semantic
focus
motion
transaction
layer
vocabulary
causal
admission
manifest
```

Rules:

- A demo may cover multiple axes.
- A demo should only claim axes that are visible in stdout evidence or the paired law document.
- `causal` coverage is governed by `vivid_causal_verdict_law_v0.md`; `AxisCausal` requires a connected evidence chain and final verdict, not just a decorative `causal_chain` field.
- Manifest smoke verifies `AxisCausal` rows have primary docs with causal evidence wording, and verifies the causal verdict law remains discoverable.
- `intent_artifact_demo` is the vertical causal anchor because it connects semantic request, state delta, invalidation, render artifact, rejection, and causal verdict.
- `semantic_transition_demo` is the first semantic-to-transaction anchor because it connects semantic request, emitted click, admission, page transaction, layer snapshot lifecycle, render sample, and causal verdict; its primary law is `vivid_semantic_transition_law_v0.md`, while `vivid_semantic_transition_evidence_v0.md` records the concrete sample stdout.
- `evidence_vocabulary_demo` is the field-law anchor because it verifies helper-derived vocabulary verdicts without claiming runtime behavior.
- `evidence_lab_manifest_demo` verifies the manifest shape and drift guards, not the runtime behavior of every listed demo.

## Non-Goals

- This manifest does not replace individual demo CTest gates.
- This manifest does not build every Evidence Lab demo.
- This manifest does not define screenshot golden files.
- This manifest does not promote demo support helpers into Vivid core.

## Maintenance Law

When adding, deleting, splitting, or renaming a CTest-gated Evidence Lab demo:

```text
1. Update vivid_evidence_stdout_law.md.
2. Update this manifest document.
3. Update Examples/ui/vivid/evidence_lab_manifest_demo.
4. Update docs/ui/README.md when the entry is a recommended route.
5. Run scripts/vivid_evidence_lab_manifest_smoke.ps1.
```

This keeps Vivid Evidence Plane from becoming a set of clever local proofs without a stable map.
