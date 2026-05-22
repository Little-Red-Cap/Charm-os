# Front Page Route Provenance Compatibility Contract v0

This contract fixes the current compatibility boundary for
`system_compiler.front_page_route/v0` route provenance entries.

It exists because older consumer-side front-page entry objects can still emit a
route-root provenance shape with legacy field names:

- `source_root_summary_path`
- `source_report_markdown_path`
- `source_check_text_path`

The route consumer now exposes one canonical front-page provenance view:

- `source_front_page_summary_path`
- `source_front_page_report_markdown_path`
- `source_front_page_check_text_path`

The compatibility boundary is intentionally narrow. It preserves old producer
outputs while keeping new route summaries and validators focused on one
consumer-facing field family.

## Scope

This contract applies only while building and validating
`system_compiler.front_page_route/v0` summaries.

The route consumer may normalize legacy route-root provenance fields into the
canonical `source_front_page_*` fields when all referenced files already exist.
The route summary remains a read-only consumer artifact.

## Compatibility Rule

For provenance entries whose `provenance_route_kind` is not
`artifact_report_index`:

- `source_front_page_summary_path` is read first.
- If it is missing, `source_root_summary_path` may be used as the legacy input.
- `source_front_page_report_markdown_path` is read first.
- If it is missing, `source_report_markdown_path` may be used as the legacy input.
- `source_front_page_check_text_path` is read first.
- If it is missing, `source_check_text_path` may be used as the legacy input.

After normalization, validators should check the canonical
`source_front_page_*` fields only. They should not require every legacy producer
to be migrated in the same change.

`artifact_report_index` remains the explicit exception. It is a discovery
provenance source, not a front-page root, so its `source_front_page_*` fields
stay empty by design.

## Consumer Boundary

The route consumer must not:

- parse raw host, QEMU, witness, or compare logs
- reopen producer internals behind a declared artifact boundary
- synthesize missing source files
- mutate the source summary that emitted legacy fields
- create a new schema kind or compare verdict
- treat provenance as a new traversal edge

The route consumer may:

- preserve `route_provenance` emitted by visited summaries
- normalize legacy route-root field names into canonical consumer fields
- validate that canonical source paths exist
- report provenance counts and route-provenance owners

## Producer Guidance

New producers should prefer the canonical `source_front_page_*` field family
when they publish front-page route provenance.

Legacy producers that still emit `source_root_summary_path`,
`source_report_markdown_path`, and `source_check_text_path` remain readable
through this compatibility boundary, but those names should not be expanded into
a second long-term route model.

## Current Regression Guard

`scripts/system_compiler_front_page_route_sample_smoke.ps1` includes a small
legacy route-root provenance fixture.

That fixture asserts that a legacy route-root provenance entry is exported with:

- one `front_page_route_root` provenance entry
- canonical `source_front_page_*` paths populated
- a non-empty front-page source summary path

The heavier opening testimony corridor smokes continue to exercise this through
real nested consumer artifacts, but the sample smoke is the first fast guard for
this compatibility seam.

## Non-Goals

This contract does not add:

- a new JSON schema
- a new validator
- a new compare brain
- a new route kind
- a new front-page entry family
- a new raw-log parser

It only fixes how existing route provenance facts are read by the generic
front-page route consumer.
