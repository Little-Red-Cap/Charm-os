# Opening Judgment Corridor Witness Taxonomy v0

> status: archived
>
> 该 taxonomy 只解释已归档 front-page/opening-flow 的历史断点语言，不是当前
> runtime、compare 或错误码契约。

`OpeningJudgmentCorridorWitnessTaxonomy` is the companion page for the
corridor's breakpoint language.

It does not define new verdicts. It names where the corridor can break and
what kind of surface or ref is missing.

The labels below are descriptive. They are not a frozen error code table.

## What this is for

- separate corridor breakpoints from compare verdicts
- keep upper layers from reinterpreting raw lower evidence
- let later docs point at the same failure family without inventing a second
  compare brain

## 1. Input / source breakpoints

- source consumer summary missing
- source compare not ok
- default focus missing
- default explain hop missing
- selected artifact ref missing from the source explain hop
- consumer summary ref missing in the bridge context

These failures mean the corridor never received a lawful opening judgment
carrier.

## 2. Selection / testimony breakpoints

- open_action blocked because required refs were missing
- opening preview does not match default focus
- no witness can be projected from the accepted open event
- landing cannot be formed from the witness surface

These failures mean a judgment exists, but it does not yet project into a
stable testimony seam.

## 3. Compare breakpoints

- compare input ref missing
- compare surface missing
- candidate surface missing
- baseline surface missing
- compare summary cannot anchor on the selected judgment path

These failures belong to the existing compare objects. The taxonomy only names
the corridor-visible failure surface.

## 4. Route breakpoints

- route root unsupported
- route provenance missing
- selected surface not declared by the landing
- supporting surface missing or not readable
- route cannot resolve the next explainable surface

These failures mean the landing was lawful, but the corridor could not produce
a valid reading route.

## 5. Explain / handoff breakpoints

- explain-entry missing
- selected summary path cannot be resolved
- explain-entry cannot be derived from route compare
- handoff target is not explainable
- handoff would reopen raw evidence instead of consuming explain-entry

These failures mean the route exists, but the final read/open action cannot be
justified.

## 6. World shelf review reading mapping

`system_compiler.world_shelf_review/v0` currently maps only onto the corridor's
reading segment:

```text
world_shelf_review
  -> front_page_route
  -> opening_testimony_explain_entry
  -> handoff
```

Its route breakpoints are:

- route root is not `system_compiler.world_shelf_review/v0`
- depth-1 `world_shelf_review` surface is missing
- shelf follow-up surfaces such as `candidate_shelf`, `shelf_compare`, or
  `baseline_shelf` are missing or unreadable

Its explain / handoff breakpoints are:

- selected review surface is missing
- selected review summary path is missing or unreadable
- handoff target does not keep the selected review surface explainable
- handoff would reopen raw shelf compare or world compare evidence instead of
  consuming the explain-entry decision

This mapping does not define a new code table and does not create a
review-specific compare brain.

## Current samples

- `runtime_session` is the first mature sample that already traverses the
  corridor through the runtime-session bridge.
- `handoff` is the current downstream terminal action of the same corridor.
- `world_shelf_review` is the first non-runtime reading-corridor sample because
  it already exposes `front_page` and `route_provenance`.

## Non-goals

- no full failure code table
- no new compare brain
- no new lower-layer judgment engine
- no schema or script changes

## Relationship to OpeningJudgmentCorridor

`OpeningJudgmentCorridor` defines the lawful transport-and-reading path.

This taxonomy only names the breakpoints on that path.

## Relationship to FrontPageReadingLaw

`FrontPageReadingLaw` remains the corridor-local reading rule name for
`front_page`, `route`, `explain_entry`, and `handoff`.

The taxonomy does not replace that alias. It only describes the places where
the corridor can fail to read.
