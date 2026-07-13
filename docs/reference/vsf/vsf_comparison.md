# VSF Comparison Guardrails

> status: `reference`
>
> scope: historical observations that require fresh Charm admission

VSF influenced early Charm exploration, but a similar directory or interface
shape is not evidence that the same abstraction belongs in Charm. Current
decisions remain subject to [`CONSTITUTION.md`](../../../CONSTITUTION.md) and
topic contracts.

## Useful Observations

- Build-time dependency checks can reject unsupported component combinations
  before runtime.
- Controller/platform hooks can be separated from protocol or device behavior.
- Fixed and dynamic queue strategies may share behavior only when ordering,
  capacity and failure semantics remain equivalent.
- Block media, filesystem and protocol-device bridges need explicit ownership
  rather than implicit type conversion.
- Driver/template tests can detect interface drift, but generated shape alone
  does not prove runtime behavior.
- Small, reproducible protocol and backend tests are more useful than broad
  framework feature claims.

## Rejected Shortcuts

- Do not import VSF's service/component/app hierarchy as Charm's global
  layering model.
- Do not introduce `Foundation`, `Component`, MAL or a generic driver model only
  because VSF uses an analogous noun.
- Do not copy macro configuration, EDA call shape, object layout or init order.
- Do not assume two queue, driver or backend implementations share semantics
  merely because one API can wrap both.
- Do not turn a third-party component inventory into a Charm roadmap.

Each candidate still needs a current consumer, explicit ownership and failure
behavior, platform-independent meaning and independent Host/QEMU/board evidence
appropriate to the claim.

Topic references:

- [`storage`](vsf_storage_map.md)
- [`TCP/IP`](vsf_tcpip_map.md)
- [`USB`](vsf_usb_map.md)
