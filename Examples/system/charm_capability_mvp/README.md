# Charm Capability MVP

## Status

- `status`: `exploration`
- `scope`: non-public cross-environment proof for the Charm MVP
- `authority`: [`CONSTITUTION.md`](../../../CONSTITUTION.md) and
  [`charm_core_contract.md`](../../../docs/architecture/charm_core_contract.md)

This example contains the shared application and composition semantics used by
the Host, real QEMU, and H747 implementation slices of the Charm MVP. It
deliberately lives outside `Modules/*`: all three domains must produce matching
runtime evidence before these semantics may request public promotion.

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
The QEMU firmware runs the same 21-case resolution matrix as Host, covering all
requirement positions and all binding-order permutations. It also runs the same
12 `app::run()` failure stages, with later capability calls stopped at the
failing boundary.

Current Host evidence passes with Clang 18.1.6 and GCC 16.1.0. The same Host
CTest set also passes a Clang 18 Release build with AddressSanitizer and
UndefinedBehaviorSanitizer enabled.
Current QEMU evidence passes with Arm GNU 17.0.0 and QEMU 10.2.90.

The H747 implementation target builds as
`h747_lab_capability_mvp`, using the same headers with a board-local Profile,
UART TextSink, tick-backed Clock adapter, and RAM BlockDevice. Its real UART log
has passed both the board capture smoke and the three-domain verifier. Host,
QEMU, and H747 all produced timestamp `424242` and checksum `0x49b880f0`; H747
also proved a missing required binding retained `start_count=0`. The types remain
exploration evidence rather than promoted public APIs.

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

Build and capture the H747 execution domain from `Examples/project/h747-lab`:

```powershell
cmake --preset h747-lab-capability-mvp-debug
cmake --build --preset build-h747-lab-capability-mvp-debug -- -j1
.\tools\capture-capability-mvp-board-smoke.ps1
```

After a passing board log exists, machine-check all three domains:

```powershell
.\Examples\system\charm_capability_mvp\verify_portable_source_boundary.ps1
.\Examples\system\charm_capability_mvp\verify_cross_environment_evidence.ps1
```

The source-boundary verifier rejects target/OS/vendor vocabulary and
conditional compilation in the shared MVP headers. It also requires the Host,
QEMU, and H747 harnesses to include the one canonical `mvp_app.hpp` exactly
once and forbids them from reopening `charm::mvp::app` to define a private app.
Its standalone default checks all three consumers; the Host/QEMU partial
verifier passes `-Domains host,qemu` explicitly so partial evidence remains
honestly scoped.
