# Minimal Kernel Runtime Session Witness Inspect Compare Consumer v0

`minimal_kernel.runtime_session_witness_inspect_compare_consumer/v0` is the
next thin consumer seam after
`minimal_kernel.runtime_session_witness_inspect_compare/v0`.

The compare object already answers:

- did the candidate witness drift from the baseline
- which runtime facts regressed or improved
- which failure codes, missing runtime facts, focus tags, and violations changed

This consumer answers the next practical question:

- which drift should a reader inspect first
- which artifact should the next explain hop open
- which failure or regression is the smallest stable explanation surface

It does not rebuild the compare object, replace world compare, or become a new
front-page route language.

It only turns one validated compare witness into an ordered set of
consumer-facing drift focuses.

## Current shape

Current `minimal_kernel.runtime_session_witness_inspect_compare_consumer`
includes:

- schema
  - `schemas/minimal_kernel.runtime_session_witness_inspect_compare_consumer.v0.schema.json`
- exporter
  - `scripts/export_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py`
- validator
  - `scripts/validate_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py`
- smoke
  - `scripts/system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1`
- inspect
  - `scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer.ps1`
- inspect smoke
  - `scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1`

## Current outputs

The exporter leaves behind:

- `session-witness.inspect.compare.consumer.summary.json`
- `session-witness.inspect.compare.consumer.report.md`
- `session-witness.inspect.compare.consumer.check.txt`

The default output root is:

```powershell
out/minimal-kernel-runtime-session-witness-inspect-compare-consumer
```

The smoke output root is:

```powershell
cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke
```

## What the consumer records

The current summary records:

- `source_compare`
  - whether the source compare changed
  - baseline/current result and session state
  - aggregate counts for runtime regressions, failure-code deltas, and violations
- `consumer_status`
  - total/changed/actionable focus counts
  - default focus id
  - highest severity
- `default_focus`
  - the first drift a higher explain layer should inspect
- `default_explain_hop`
  - the first artifact a higher explain layer should open
  - why that artifact is preferred over sibling surfaces
- `fallback_explain_hops`
  - ordered alternative explain hops if the default opening is not enough
- `focus_entries`
  - ordered drift units such as session-state drift, runtime regressions,
    world-compare drift, witness-compare drift, or violation drift
  - each focus also carries a `preferred_explain_hop`
- `readiness_surface`
  - focus-kind counters
  - severity counters
  - changed/actionable focus ids
- `supporting_artifacts`
  - stable artifact refs for current summary, session summary, runtime ledger,
    world compare, and witness compare surfaces

The runtime ledger artifact follows `docs/system/minimal_kernel_runtime_ledger_fact_contract_v0.md`.
The consumer may expose it as a supporting artifact or explain-hop context, but it must not reinterpret raw host/QEMU/session logs or replace the session verdict.

Each focus entry keeps:

- focus id / kind / severity / priority
- baseline/current session status and failure domain
- runtime regressions and improvements
- added/removed failure codes
- added/removed missing runtime facts
- added/removed affected focus tags
- added/removed violations
- artifact refs
- preferred explain hop
- short summary lines and next-step questions

Each explain hop keeps:

- focus id / focus kind
- selected artifact ref
- fallback artifact refs
- reason kind / reason
- copied headline / summary lines / question lines

## Current policy

The consumer deliberately stays below a full explain surface.

It does not choose arbitrary render trees or invent a second compare language.
It only compresses one inspect-compare object into a thinner answer:

- inspect the session-state drift first if the session result or status moved
- inspect runtime regressions next if continuity facts regressed
- keep world-compare and witness-compare deltas as separate focus surfaces
- keep summary-level violations as the last thin gate-facing focus
- explicitly point the reader at the first artifact to open for each focus
- preserve a small fallback list instead of forcing the next layer to guess

That keeps this layer stable and useful without hard-coding later UI policy.

## Manual example

Bootstrap the source compare witness first:

```powershell
./scripts/inspect_minimal_kernel_runtime_session_witness_compare_summary_smoke.ps1 `
  -OutputRoot cmake-build-minimal-kernel-runtime-session-witness-compare-summary-smoke
```

Then export the consumer:

```powershell
python ./scripts/export_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py `
  --compare cmake-build-minimal-kernel-runtime-session-witness-compare-summary-smoke/session-witness.inspect.compare.summary.json `
  --output-root cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke
```

Validate it:

```powershell
python ./scripts/validate_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py `
  --summary cmake-build-minimal-kernel-runtime-session-witness-inspect-compare-consumer-smoke/session-witness.inspect.compare.consumer.summary.json
```

Or run the single smoke:

```powershell
./scripts/system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1 -Clean
```

Or guard the direct inspect entry itself against an already-exported consumer summary:

```powershell
./scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1
./scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1 -Summary out/minimal-kernel-runtime-session-witness-inspect-compare-consumer/session-witness.inspect.compare.consumer.summary.json
```

Or inspect the consumer directly:

```powershell
./scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer.ps1
./scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer.ps1 -Summary out/minimal-kernel-runtime-session-witness-inspect-compare-consumer/session-witness.inspect.compare.consumer.summary.json -ShowFallbacks
./scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer.ps1 -Summary out/minimal-kernel-runtime-session-witness-inspect-compare-consumer/session-witness.inspect.compare.consumer.summary.json -FocusId runtime-regression -ShowArtifacts
./scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer.ps1 -Summary out/minimal-kernel-runtime-session-witness-inspect-compare-consumer/session-witness.inspect.compare.consumer.summary.json -FocusId world-compare-drift -AsJson
```

Expected smoke shape:

```text
[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER-SMOKE] focuses=5 changed=5 default=session-state-drift severity=critical next=session-report
==> inspect minimal kernel runtime session witness inspect compare consumer smoke
selected_focus=world-compare-drift selected_explain_hop=world-compare-report ok=1
```

When the summary already exists, the inspect smoke can also reuse it directly instead of rebuilding the consumer:

```text
[MINIMAL-KERNEL-RUNTIME-SESSION-WITNESS-INSPECT-CONSUMER-SMOKE] bootstrap=reuse-existing-summary
==> inspect minimal kernel runtime session witness inspect compare consumer smoke
selected_focus=world-compare-drift selected_explain_hop=world-compare-report ok=1
```

The session witness CI entry now uses exactly this reuse path after exporting the consumer summary. That keeps the guardrail thin:

- `ci_minimal_kernel_runtime_session_witness_smoke.ps1`
  exports the compare consumer
- `inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1 -Summary ...`
  immediately proves the direct inspect seam still opens the expected explain hop

## Why this matters

`minimal_kernel.runtime_session_witness_inspect_compare/v0` proves the drift.

`minimal_kernel.runtime_session_witness_inspect_compare_consumer/v0` makes that
drift directly consumable.

This gives later explain/front-page layers a thinner first-read artifact:

- start from this one default focus
- open the default explain hop without reparsing artifact refs
- keep runtime regressions and failure taxonomy deltas separate
- follow the fallback explain hops only when the default opening is insufficient
- avoid reparsing baseline/candidate summaries just to answer “what should I
  look at first?”
