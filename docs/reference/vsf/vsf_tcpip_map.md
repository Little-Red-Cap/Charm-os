# VSF TCP/IP Structure Reference

> status: `reference`
>
> scope: historical VSF netdrv, socket and protocol decomposition

This note records third-party structure observations, not a Charm network
roadmap. Current Charm network status and semantics start at
[`docs/io/README.md`](../../io/README.md) and
[`net_socket_v0_contract.md`](../../io/net_socket_v0_contract.md).

## VSF Decomposition

| VSF area | Responsibility |
|---|---|
| `netdrv` | network-device/link adaptation and netif handoff |
| `socket` | socket object and backend operation table |
| `protocol` | higher-level clients such as HTTP |

Historical netdrv objects separate link output from stack-adapter buffer and
input handling. They also expose device lifecycle/connect callbacks. Historical
socket operations cover open/close, bind/listen, connect/accept, send/receive
and name resolution, with lwIP and host backends.

## Transferable Observations

- MAC/PHY or host capture ownership is distinct from stack/netif ownership.
- Packet buffer allocation and return paths need explicit lifetime rules.
- Link up/down and device creation are observable lifecycle events, not just
  successful socket calls.
- A socket facade must preserve backend errors and partial-transfer semantics.
- Protocol clients should consume the socket contract rather than a specific
  lwIP or host implementation.

## What Does Not Transfer Automatically

- VSF object names, operation tables and callback threading;
- PnP or adapter semantics without a current Charm consumer;
- one interface pretending lwIP and host sockets have identical blocking,
  cancellation and readiness behavior;
- a new packet/netif abstraction before ownership and fixed-capacity behavior
  are proven;
- the historical migration order or proposed directory tree.

This reference does not prove that Charm has a complete network stack, DNS,
HTTP client, MAC/PHY backend or cross-platform socket implementation.
