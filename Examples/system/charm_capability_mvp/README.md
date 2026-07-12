# Charm Capability MVP

## Status

- `status`: `exploration`
- `scope`: non-public Host proof for the Charm cross-environment MVP
- `authority`: [`CONSTITUTION.md`](../../../CONSTITUTION.md) and
  [`charm_core_contract.md`](../../../docs/architecture/charm_core_contract.md)

This example contains the Host and real QEMU implementation slices of the Charm
MVP. It deliberately lives outside `Modules/*`: the semantics must also survive
a real board before they may request public promotion.

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

The Host failure matrix additionally exercises every MVP requirement position
for missing, duplicate, out-of-range, contract-mismatched, and invalid
provisions, plus every `app::run()` failure stage from Clock through report
flush. It checks failure-stage Evidence flags and confirms later providers are
not called after an earlier stage fails. This is exploration evidence, not a
public API promotion.

The QEMU slice builds a Cortex-M7 firmware and runs it inside QEMU `mps2-an500`.
It consumes the same `mvp_app.hpp`, contract definitions, and resolver as Host;
only the Profile and providers change. It produces the same timestamp and record
checksum and proves a missing binding stops before App start inside the firmware.

Current Host evidence passes with Clang 18.1.6 and GCC 16.1.0. The same Host
CTest set also passes a Clang 18 Release build with AddressSanitizer and
UndefinedBehaviorSanitizer enabled.
Current QEMU evidence passes with Arm GNU 17.0.0 and QEMU 10.2.90.

This still does not prove real-board execution. Host-only board metadata checks
are not accepted as substitutes.

Build and run:

```powershell
cmake -S Examples/system/charm_capability_mvp -B Examples/system/charm_capability_mvp/cmake-build-charm-capability-mvp -G Ninja
cmake --build Examples/system/charm_capability_mvp/cmake-build-charm-capability-mvp
ctest --test-dir Examples/system/charm_capability_mvp/cmake-build-charm-capability-mvp --output-on-failure
```

Run the real QEMU firmware smoke:

```powershell
.\Examples\system\charm_capability_mvp\qemu\run_qemu_ci.ps1
```
