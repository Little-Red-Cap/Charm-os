# Charm Capability MVP

## Status

- `status`: `exploration`
- `scope`: non-public Host proof for the Charm cross-environment MVP
- `authority`: [`CONSTITUTION.md`](../../../CONSTITUTION.md) and
  [`charm_core_contract.md`](../../../docs/architecture/charm_core_contract.md)

This example is the first implementation slice of the Charm MVP. It deliberately
lives outside `Modules/*`: the semantics must survive Host, real QEMU firmware,
and a real board before they may request public promotion.

The portable app in `mvp_app.hpp` declares exactly three requirements:

- `TextSink / report`
- `Clock / monotonic_time`
- `BlockDevice / record_store`

The app only receives a resolved context. It does not contain a platform macro,
vendor header, HAL handle, target name, profile name, or provider identity.

The Host harness proves:

- one shared definition for each capability contract projection;
- explicit Requirement, Provision, and Binding values;
- successful resolution before app start;
- stable pre-start failure for missing, duplicate, contract-mismatched, and invalid provisions;
- a timestamped record write/read check and TextSink report;
- no app start after a resolution failure.

It does not yet prove QEMU or real-board execution. Host-only QEMU/board metadata
checks are not accepted as substitutes.

Current Host evidence passes with Clang 18.1.6 and GCC 16.1.0.

Build and run:

```powershell
cmake -S Examples/system/charm_capability_mvp -B Examples/system/charm_capability_mvp/cmake-build-charm-capability-mvp -G Ninja
cmake --build Examples/system/charm_capability_mvp/cmake-build-charm-capability-mvp
ctest --test-dir Examples/system/charm_capability_mvp/cmake-build-charm-capability-mvp --output-on-failure
```
