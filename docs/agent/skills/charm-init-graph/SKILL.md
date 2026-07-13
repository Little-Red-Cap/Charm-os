---
name: charm-init-graph
description: 审查或接入已经选择 init.graph 的静态初始化目标。
---

# Charm Init Graph

> status: `supporting`

Use this skill only when the affected target already uses `init::Graph`, or when
source evidence shows a static dependency graph is required. It does not require
every board capability, dynamic device or application to adopt init.graph.

## Before Editing

1. Read the target/profile CMake, startup path and current nodes.
2. Identify the consumer, owner and initialization lifetime.
3. Read [`init_graph_contract.md`](../../../system/init_graph_contract.md) and
   the actual `init.node`/`init.graph` source.
4. Decide whether the dependency is static initialization. Dynamic discovery,
   hot-plug and runtime lookup need a different owner.

## Wiring

- Define the smallest node with explicit `provides` and `requires_caps`.
- Choose `runlevel_mask` and one real `Phase` (`early`, `core`, `service`,
  `app`) from target behavior, not a generic platform/HAL/driver hierarchy.
- Keep each selected capability to one provider; do not add fallback selection
  to hide duplicate or missing wiring.
- Bind the node in the owning target/profile rather than an unrelated global
  entry.
- Register a channel/block/device endpoint only when the consumer uses the
  corresponding registry contract. Registry insertion is not an init.graph
  requirement.
- Keep board/HAL handles and vendor state in the project/backend adapter.

CapId strings are hashed identifiers used by the graph. Centralize them with
their owner and test duplicates/collisions relevant to the target; a string's
presence does not make it a global Charm capability namespace.

## Validation

Cover the applicable paths directly:

- selected positive graph order and init calls;
- missing and duplicate provider;
- zero capability ID, capacity overflow and phase inversion;
- dependency cycle or self-dependency;
- runlevel/max-phase filtering;
- first init failure stopping later nodes, with no inferred rollback/deinit.

Also run the real target when the claim includes platform startup. Host graph
tests cannot prove IRQ, clock, peripheral or board readiness.

## Review Output

State the node owner, selected target, provides/requires, phase/runlevel,
failure behavior and validation domain. Do not describe directory layering or a
successful configure as proof that initialization completed.
