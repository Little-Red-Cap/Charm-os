# System Compiler Front Page Entry Opening Flow Open Event v0

`system_compiler.front_page_entry_opening_flow_open_event/v0` is the first
hard explanation record for the opening-flow chain.

It is also the concrete carrier of `OpeningJudgment v0`.

The file name stays `open_event` because this object is still anchored to one
front-page entry opening. The semantic role is now explicit:

```text
open_event
  = concrete OpeningJudgment carrier
open_event_witness
  = testimony projection of that judgment
open_event_compare
  = semantic drift judge for two opening judgments
open_event_witness_compare
  = testimony drift judge for two compact testimonies
```

It sits after:

- `system_compiler.front_page_entry_opening_flow_consumer_plan_action/v0`
- optional `system_compiler.front_page_entry_opening_flow_consumer_plan_action_compare/v0`

It can also be produced by the runtime-session-specific wrapper after:

- `minimal_kernel.runtime_session_witness_inspect_compare_consumer/v0`
- `system_compiler.front_page_entry_runtime_session_opening_flow_plan_action/v0`

The older action facade answers:

- which already-planned action should a consumer open now

The open-event facade answers the larger Charm question:

- why this opening happened
- which semantic consumer/action was selected
- which candidate consumers were not selected
- which plan/action promise was executed
- whether compare context changes the opening judgment
- which witness refs make the judgment auditable
- which minimal explain view should be presented beside the workspace facade

This is intentionally not a rich UI shell yet.

It is a deterministic opening judgment witness.

## Current shape

Current `system_compiler.front_page_entry_opening_flow_open_event` includes:

- schema
  - `schemas/system_compiler.front_page_entry_opening_flow_open_event.v0.schema.json`
- exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_open_event.py`
- runtime-session wrapper exporter
  - `scripts/export_system_compiler_front_page_entry_runtime_session_opening_flow_open_event.py`
- workspace exporter
  - `scripts/export_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1`
- validator
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_open_event.py`
- witness
  - `scripts/export_system_compiler_front_page_entry_opening_flow_open_event_witness.py`
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py`
- witness compare
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness.py`
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_witness_workspace.ps1`
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness_compare.py`
- compare
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_open_event.py`
  - `scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1`
  - `scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_compare.py`
- smoke
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_workspace_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_workspace_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_opening_flow_open_event_workspace_compare_smoke.ps1`
  - `scripts/system_compiler_front_page_entry_runtime_session_open_event_witness_smoke.ps1`

## Current outputs

The exporter leaves behind:

- `front-page.entry-opening-flow.open-event.summary.json`
- `front-page.entry-opening-flow.open-event.report.md`
- `front-page.entry-opening-flow.open-event.check.txt`

The witness exporter leaves behind:

- `front-page.entry-opening-flow.open-event.witness.summary.json`
- `front-page.entry-opening-flow.open-event.witness.report.md`
- `front-page.entry-opening-flow.open-event.witness.check.txt`

The default output root is:

```powershell
out/system-compiler-front-page-entry-opening-flow-open-event
```

The workspace wrapper writes under:

```powershell
out/system-compiler-open-event-ws
```

The smoke output roots are:

```powershell
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-workspace-smoke
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-smoke
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-compare-smoke
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-witness-workspace-compare-smoke
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-compare-smoke
cmake-build-system-compiler-front-page-entry-opening-flow-open-event-workspace-compare-smoke
```

## What the open event records

The current summary records:

- `open_event`
  - stable `open_event_id`
  - opening status: `accepted`, `accepted_with_drift`, or `blocked`
  - structured opening reason
  - source artifact and opener surface paths
  - optional `opening_input_refs`
    - `consumer_summary_ref`
    - `selected_focus_ref`
    - `selected_explain_hop_ref`
    - `selected_artifact_ref`
    - `fallback_artifact_refs`
- `consumer_decision`
  - selected consumer projection
  - candidate consumers projected from the source plan actions
  - rejected consumer reasons for non-selected plan actions
  - decision reason copied from the selected action facade
- `plan`
  - source plan status
  - action count
  - default, compare, and selected action ids
- `action_records`
  - selected action expected operation
  - selected action result
  - attached compare result when available
- `compare_summary`
  - optional action compare verdict
  - changed field count
  - reason drift marker and narratives
- `workspace_facade`
  - the minimal projected explain-open workspace facade
- `diagnostic_preview`
  - the selected action's first-read diagnostic headline
  - projection summary lines and question lines copied from the source
    `opening_preview`
  - line counts and preview blockers, so explain/UI consumers do not need to
    reopen the source plan action just to render the first diagnostic card
- `witness_refs`
  - source action witness
  - selected opener witness
  - open-event witness
  - optional action-compare witness
- `judgment`
  - `semantic_role=opening_judgment_carrier`
  - judgment status, grade, basis, accepted flag, and a short system-testimony
    summary
- `explanation_view`
  - hard text lines for `why opened`, `chosen consumer`, `plan actions`, `compare result`, and `witness refs`
- `questions`
  - existing string questions
  - additive typed next-question hints for later front-page consumption

## Judgment carrier v0

The top-level `judgment` object is a summary of facts already present in the
open event.

It does not replace `open_event.status`, `compare_summary`, or
`witness_refs`.

Current fields are:

- `semantic_role`
  - fixed to `opening_judgment_carrier`
- `status`
  - mirrors `open_event.status`
- `grade`
  - `described` when no action compare is attached
  - `compared` when an action compare is attached
- `basis`
  - always includes `source_plan_action`, `selected_opener`, and `open_event`
  - also includes `source_action_compare` when compare context is attached
- `accepted`
  - true unless the event is `blocked`
- `summary`
  - a short, restrained system-testimony sentence suitable for reports and
    later explain surfaces

This version deliberately does not emit `witnessed`.

The witness is downstream of the open event. A later witness-bundle or
witness-compare carrier can lift a judgment to `witnessed` without changing the
open-event root object.

## Event status

The event status is deliberately small:

- `accepted`
  - selected action is ready and no drift compare is attached
- `accepted_with_drift`
  - selected action is ready, but attached action compare reports `drifted` or `collapsed`
- `blocked`
  - selected action is not ready, source result is not ok, or blockers are present

This lets the explain surface distinguish a clean opening from an opening that
should foreground counterfactual context before presenting the workspace.

## Typed next questions

The string question arrays remain the compatibility surface:

- `questions.open_event_questions`
- `questions.next_questions`

`questions.typed_next_questions` is additive.

The first version emits:

- `inspect_action_compare`
  - when action compare context is attached
- `attach_action_compare`
  - when the open event is only described
- `inspect_rejected_consumers`
  - always emitted so front-page tooling can point at rejected consumer reasons

These hints are not yet part of open-event compare drift semantics. Compare v0
continues to judge the established event, consumer, plan, action, compare,
workspace, witness, and hard explanation fields.

## Consumer decision v0

This version does not invent a global consumer registry.

Instead, it projects consumer candidates from the existing plan actions:

```text
selected:
  delivery_biography:default_overview:report

rejected:
  counterfactual_verdict:default_overview:artifact_root
    reason: compare neighbor stayed available but selector default_action chose another action
```

That is enough to make the first counterfactual opening explanation visible
without forcing a premature registry design.

Later, these projected consumers can be replaced by real consumer registry
entries while keeping the same open-event shape.

The runtime-session wrapper deliberately stays narrower than the generic plan
path:

- one candidate consumer
- zero rejected consumers
- fallback explain hops stay in `opening_input_refs`
- the selected artifact target is explicit, but the primary workspace facade
  still points at the consumer-facing explain surface

This keeps the opening judgment explainable without letting upper layers
reinterpret raw session evidence.

## Compare v0

`system_compiler.front_page_entry_opening_flow_open_event_compare/v0` compares
two opening judgments.

It is not a raw JSON diff.

The compare normalizes paths produced under each open-event output root, so the
event's own output location does not count as semantic drift.

The current compare judges:

- event status and reason
- selected consumer/action
- candidate and rejected consumer surfaces
- plan/action record promises
- attached compare context
- workspace facade target
- witness ref roles and summary refs
- diagnostic preview headline / summary lines / question lines
- hard explanation text

That lets the system answer questions like:

```text
Did adding compare context change this opening judgment?
Did this opening select a different consumer?
Did the workspace facade or witness set drift?
```

The current verdicts are:

- `standing`
  - the opening judgment did not semantically change
- `improved`
  - a previously drift-aware opening returns to a clean accepted opening
- `drifted`
  - the opening judgment changed but still produces an accepted event
- `collapsed`
  - the candidate opening no longer produces an accepted event

## Manual example

Run the open-event smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_smoke.ps1 -Clean
```

Run the workspace open-event smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_workspace_smoke.ps1 -Clean
```

Run the open-event compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_compare_smoke.ps1 -Clean
```

Run the open-event witness smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_smoke.ps1 -Clean
```

Run the open-event witness compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_compare_smoke.ps1 -Clean
```

Run the open-event witness workspace compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_witness_workspace_compare_smoke.ps1 -Clean
```

Export directly from an action summary:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_open_event.py `
  --action cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke/cold-default/action/front-page.entry-opening-flow.consumer.plan-action.summary.json `
  --output-root cmake-build-open-event-direct
```

Export with an attached action compare:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_open_event.py `
  --action cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke/cold-default/action/front-page.entry-opening-flow.consumer.plan-action.summary.json `
  --action-compare cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-compare-smoke/default-to-compare-neighbor/front-page.entry-opening-flow.consumer.plan-action.compare.summary.json `
  --output-root cmake-build-open-event-with-compare
```

Export through the workspace wrapper:

```powershell
./scripts/export_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1 `
  -ActionSummaryPath cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke/cold-default/action/front-page.entry-opening-flow.consumer.plan-action.summary.json `
  -OutputRoot cmake-build-open-event-ws
```

Or create the selected action from an existing plan workspace first:

```powershell
./scripts/export_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1 `
  -PlanWorkspaceRoot cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke/cold-default/plan-ws `
  -ActionKind compare-neighbor `
  -OutputRoot cmake-build-open-event-compare-neighbor-ws
```

Then validate:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_open_event.py `
  --summary cmake-build-open-event-direct/front-page.entry-opening-flow.open-event.summary.json
```

Export an OpenEventWitness from an open event:

```powershell
python ./scripts/export_system_compiler_front_page_entry_opening_flow_open_event_witness.py `
  --open-event cmake-build-open-event-direct/front-page.entry-opening-flow.open-event.summary.json `
  --output-root cmake-build-open-event-witness
```

Then validate the witness:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_witness.py `
  --summary cmake-build-open-event-witness/front-page.entry-opening-flow.open-event.witness.summary.json
```

Compare two open events:

```powershell
python ./scripts/compare_system_compiler_front_page_entry_opening_flow_open_event.py `
  --baseline cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke/default-no-compare/front-page.entry-opening-flow.open-event.summary.json `
  --candidate cmake-build-system-compiler-front-page-entry-opening-flow-open-event-smoke/default-with-drift-compare/front-page.entry-opening-flow.open-event.summary.json `
  --output-root cmake-build-open-event-compare
```

Then validate the compare:

```powershell
python ./scripts/validate_system_compiler_front_page_entry_opening_flow_open_event_compare.py `
  --summary cmake-build-open-event-compare/front-page.entry-opening-flow.open-event.compare.summary.json
```

Compare two opening judgments through the workspace wrapper:

```powershell
./scripts/compare_system_compiler_front_page_entry_opening_flow_open_event_workspace.ps1 `
  -BaselineOpenEventWorkspaceRoot cmake-build-system-compiler-front-page-entry-opening-flow-open-event-workspace-smoke/from-action-summary `
  -CandidateActionSummaryPath cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-workspace-smoke/cold-default/action/front-page.entry-opening-flow.consumer.plan-action.summary.json `
  -CandidateActionCompareSummaryPath cmake-build-system-compiler-front-page-entry-opening-flow-consumer-plan-action-compare-smoke/default-to-compare-neighbor/front-page.entry-opening-flow.consumer.plan-action.compare.summary.json `
  -OutputRoot cmake-build-open-event-workspace-compare `
  -Clean
```

Run the workspace compare smoke:

```powershell
./scripts/system_compiler_front_page_entry_opening_flow_open_event_workspace_compare_smoke.ps1 -Clean
```

Expected smoke shape:

```text
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-SMOKE] case=default-no-compare status=accepted compare=False/not_attached
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-SMOKE] case=default-with-drift-compare status=accepted_with_drift compare=True/drifted
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-SMOKE] case=from-action-summary status=accepted action=open-default
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-SMOKE] case=from-plan-workspace-compare-neighbor status=accepted action=open-compare-neighbor
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-SMOKE] case=default-no-compare-witness witness_status=ok event_status=accepted compare=False/not_attached
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-SMOKE] case=default-with-drift-compare-witness witness_status=ok event_status=accepted_with_drift compare=True/drifted
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] case=open-event-witness-self-standing verdict=standing changed=0
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-COMPARE-SMOKE] case=open-event-witness-default-to-drift-context verdict=drifted changed=17
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] case=workspace-witness-summary-self-standing verdict=standing changed=0
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WITNESS-WORKSPACE-COMPARE-SMOKE] case=workspace-open-event-summary-to-drift-witness verdict=drifted changed=17
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-COMPARE-SMOKE] case=open-event-self-standing verdict=standing changed=0
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-COMPARE-SMOKE] case=open-event-default-to-drift-context verdict=drifted changed=13
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE-SMOKE] case=workspace-self-standing verdict=standing changed=0 status_changed=False compare_changed=False witness_changed=False
[FRONT-PAGE-ENTRY-OPENING-FLOW-OPEN-EVENT-WORKSPACE-COMPARE-SMOKE] case=workspace-action-summary-to-drift-context verdict=drifted changed=13 status_changed=True compare_changed=True witness_changed=True
```

## Why this matters

The opening-flow chain now has one more semantic layer:

- selector
  - decide which explain entries matter first
- plan
  - turn that order into a small execution bundle
- action
  - choose one deterministic opener witness to execute now
- open event
  - carry the concrete OpeningJudgment, explain why it is valid, what was rejected, what was compared, and what witness refs preserve it
- open event witness
  - distill the opening judgment into a compact testimony object for later bundle and constitution work
- open event witness compare
  - judge whether two compact testimonies still prove the same opening judgment
- open event compare
  - judge whether two opening judgments preserve the same explainable opening semantics

This is the first minimal form of `Explainable Opening v0`.

Charm no longer only opens a surface.

It can now leave behind a witness that explains why the surface was opened.
