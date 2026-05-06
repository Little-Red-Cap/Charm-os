# Opening Judgment Corridor v0

`OpeningJudgmentCorridor` is the first explicit contract for the shared reading
path that starts from a lower-layer opening judgment and ends at an upper-layer
default explain/handoff decision.

It is a contract-level subject, not a new exporter stack.

In this phase, `FrontPageReadingLaw` is only an alias for the corridor's
`front_page / explain_entry` reading rules. It is not a second peer object.

## Position

`OpeningJudgmentCorridor` answers one question:

> Once a lower seam has already decided what should be read, how does that
> judgment travel upward without being retried, reinterpreted, or reopened from
> raw evidence?

The corridor is therefore a law of transport and reading, not a new judgment
engine.

## Corridor shape

The current full chain is:

```text
consumer
  -> plan_action
  -> open_event
  -> witness
  -> landing
  -> route
  -> explain_entry
  -> handoff
```

This contract freezes the chain as three layers:

### 1. Judgment production

```text
consumer
  -> plan_action
  -> open_event
```

This layer decides what should be read now.

### 2. Testimony projection

```text
open_event
  -> witness
  -> landing
```

This layer turns the concrete opening judgment into a portable testimony and a
stable explain-entry landing seam.

### 3. Reading corridor

```text
landing
  -> route
  -> explain_entry
  -> handoff
```

This layer decides how the already-exported testimony should be read, routed,
opened, and handed off.

## Hard boundaries

The corridor keeps five hard boundaries.

### 1. Upper layers must not retry lower judgments

Once a lower seam has exported a lawful judgment carrier, upper layers must not
reopen raw runtime/session/world-compare evidence to invent a second decision.

### 2. `front_page_route` only consumes lawful landing/route surfaces

The route layer may walk declared `front_page` and `route_provenance` surfaces,
but it must not bypass them and rediscover lower graph structure on its own.

### 3. `opening_testimony_explain_entry` only consumes route or route-compare

The explain-entry layer is allowed to read route summaries and route-compare
summaries. It is not allowed to reopen raw witness/session evidence.

### 4. `handoff` only consumes explain-entry

The handoff layer may only consume a lawful explain-entry decision, and its
open target must itself still be an explainable surface rather than a naked
artifact jump.

### 5. compare reuses existing compare objects

This corridor does not introduce a second compare brain. Existing compare
objects keep their own verdict semantics; the corridor only constrains how
their already-exported decisions can continue upward.

## Non-goals

This v0 does not try to become:

- a new lower-half opening judgment engine
- a replacement selected-surface selector
- a replacement compare framework
- a handoff UI policy
- a new schema family or script family

It is a constitutional contract for the reading path only.

## Current mature samples

### Runtime session

`runtime_session` is the first mature sample that enters the corridor through a
dedicated bridge:

```text
runtime session witness compare consumer
  -> runtime-session opening-flow plan action bridge
  -> runtime-session open-event wrapper
  -> standard open_event
  -> standard open_event_witness
  -> opening_testimony_landing
  -> front_page_route
  -> opening_testimony_explain_entry
  -> opening_testimony_explain_entry_handoff
```

This sample proves that the corridor can carry a lower runtime judgment without
letting upper layers rebuild a second runtime/session compare policy.

### Handoff

`handoff` is the current downstream terminal action of the same corridor.

It proves that the upper seam can receive an explain-entry decision and turn it
into one deterministic open instruction without reordering or reinterpretation.

## First non-runtime expansion target

`system_compiler.world_shelf_review/v0` is the first explicit non-runtime
expansion target.

It is not yet a corridor-specific carrier, but it already exposes the two key
ingredients the corridor needs:

- machine-readable `front_page`
- machine-readable `route_provenance`

The next expansion is therefore not to create a separate review-only upper
brain, but to let review-side reading reuse the same:

```text
landing
  -> route
  -> explain_entry
  -> handoff
```

lawful path.

## Relationship to FrontPageReadingLaw

`OpeningJudgmentCorridor` is the structural subject.

`FrontPageReadingLaw` is only the name for the corridor's upper reading rules
inside:

- `front_page`
- `route`
- `explain_entry`
- `handoff`

In other words:

```text
OpeningJudgmentCorridor
  = full transport-and-reading law

FrontPageReadingLaw
  = corridor-local reading law for front-page-facing layers
```

## Future work

Later work may add a dedicated corridor witness taxonomy for:

- collapse points
- drift points
- unsupported reading roots
- missing explain targets

This v0 does not freeze a full code table yet.

It only reserves the law that any future taxonomy must describe corridor
breakpoints, not create a second upper-layer judgment engine.
