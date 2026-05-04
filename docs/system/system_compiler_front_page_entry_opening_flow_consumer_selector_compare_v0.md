# System Compiler Front Page Entry Opening Flow Consumer Selector Compare v0

`system_compiler.front_page_entry_opening_flow_consumer_selector_compare/v0`
compares two
`system_compiler.front_page_entry_opening_flow_consumer_selector/v0`
summaries.

It answers one question:

- did the selected explain-open order preserve the same first-read surface

The compare object does not rerun the opening-flow chain.

It consumes two existing selector summaries and leaves a deterministic compare
witness.

## Current shape

Current
`system_compiler.front_page_entry_opening_flow_consumer_selector_compare`
includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_consumer_selector_compare.v0.schema.json`
- exporter
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_consumer_selector.py`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_selector_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_consumer_selector_compare_smoke.ps1`

## Current outputs

By default the compare exporter leaves behind:

- `front-page.entry-opening-flow.consumer.selector.compare.summary.json`
- `front-page.entry-opening-flow.consumer.selector.compare.report.md`
- `front-page.entry-opening-flow.consumer.selector.compare.check.txt`

The dedicated smoke writes under:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-compare-smoke
```

## What the compare records

The current summary records:

- baseline and candidate selector summary paths
- baseline and candidate selector provenance
- selector-level result, plan status, selected count, fallback count, default
  entry, and compare entry changes
- ordered selected-entry additions, removals, and order drift
- selected-entry changes for tab, query, target, projection, compare context,
  and inspector readiness
- regression surface narratives
- a verdict:
  - `standing`
  - `improved`
  - `drifted`
  - `collapsed`

## Current smoke evidence

The smoke currently proves two paths:

- `self-standing`
  - compares the selector summary with itself
  - expected verdict: `standing`
- `removed-compare-entry`
  - removes `root-witness-to-root-world-compare` from a synthetic candidate
  - expected verdict: `drifted`

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-COMPARE-SMOKE] case=self-standing verdict=standing changed=0 added=0 removed=0 default_changed=False compare_changed=False
[FRONT-PAGE-ENTRY-OPENING-FLOW-CONSUMER-SELECTOR-COMPARE-SMOKE] case=removed-compare-entry verdict=drifted changed=8 added=0 removed=1 default_changed=False compare_changed=True
```

## Boundary

This object intentionally stays at selector-compare level.

It does not:

- rebuild consumer handoff summaries
- rerun opener, landing, capability, or route tools
- execute inspector commands
- invent a separate consumer plan language

It only judges whether two selector witnesses expose the same explain-open
order.

For workspace-exported selectors, target and opener paths are compared relative
to the nearest consumer workspace root. That keeps scratch output roots from
looking like explain-order drift while still preserving original absolute paths
in provenance fields.

## Manual example

Generate the base selector witness:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_selector_smoke.ps1 -Clean
```

Run the compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_consumer_selector_compare_smoke.ps1 -Clean
```

Or compare two explicit selector summaries:

```powershell
python ./scripts/compare_system_compiler_front_page_entry_opening_flow_consumer_selector.py `
  --baseline cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-smoke/front-page.entry-opening-flow.consumer.selector.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-smoke/front-page.entry-opening-flow.consumer.selector.summary.json `
  --output-root cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-compare-smoke/self-standing
```

Validate the compare object:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_consumer_selector_compare.py `
  --summary cmake-build-system-compiler-front-page-entry-opening-flow-consumer-selector-compare-smoke/self-standing/front-page.entry-opening-flow.consumer.selector.compare.summary.json
```

## Why this matters

`front_page_entry_opening_flow_consumer_selector/v0` made the opening handoff
directly selectable.

`front_page_entry_opening_flow_consumer_selector_compare/v0` makes that
selection interrogable across time.

Instead of asking a later explain tool to diff selector JSON, it can consume one
object that says:

- did the first explain entry change
- did the compare neighbor move or disappear
- did fallback order drift
- did a projection, compare context, or inspector readiness boundary change
- which selected entry changed first

That keeps the front-page explain line close to Charm's larger rule: every
surface that can be opened should also be able to testify about how it changed.
