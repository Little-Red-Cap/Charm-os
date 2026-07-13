# init.graph Contract

> status: `supporting`
>
> authority: [`init.graph.cppm`](../../Modules/core/init/init.graph.cppm) and
> [`init.node.cppm`](../../Modules/core/init/init.node.cppm)

`init::Graph<MaxNodes, MaxCaps>` builds a fixed-capacity initialization order
from explicit nodes and capability dependencies. It is not a Service/Component
registry and does not discover dependencies from directories or types.

## Selection

`build(nodes, runlevel_mask, max_phase)` first resets previous graph state, then
selects non-null nodes whose runlevel intersects the mask and whose phase is not
greater than `max_phase`. Dependencies are resolved only within that selected
set; filtering out a provider therefore produces `noent` for a selected
consumer.

## Validation And Errors

| Condition | Result |
|---|---|
| input node count exceeds `MaxNodes` | `buffer_overflow` |
| provided or required `CapId` is zero | `invalid_arg` |
| two selected nodes provide the same capability | `exist` |
| selected capabilities exceed `MaxCaps` | `buffer_overflow` |
| required capability has no selected provider | `noent` |
| node requires its own capability | `bad_state` |
| provider phase is later than consumer phase | `bad_state` |
| dependency cycle prevents complete ordering | `bad_state` |

Each capability has exactly one selected provider. A missing or duplicate
provider is a build failure; the graph does not select a fallback.

## Ordering And Start

`build()` performs one Kahn topological sort and stores the resulting node
order in fixed arrays. Nodes with equal dependency freedom have the order
produced by the input scan and queue; no stronger priority guarantee is stated.

`start()` calls non-null `init(ctx)` functions in stored order. It stops at the
first failed `Result` and returns that error. Current `Graph` does not call
`deinit`, roll back already-started nodes or enforce non-blocking behavior.
Those policies must not be inferred from the presence of `Node::deinit`.

`Graph` contains no dynamic allocation. Capacity, dependency and start failure
paths require direct tests in the target evidence domain; a successful CMake
configure does not prove runtime initialization.
