# H747 Capability MVP

## Status

- `status`: `exploration`
- `scope`: real-board implementation slice for the Charm cross-environment MVP
- `public API`: none

This app is the H747 environment harness for the portable source under
`Examples/system/charm_capability_mvp`. It does not redefine the application,
contracts, resolver, or evidence format.

The board changes only the Profile and providers:

- `TextSink`: blocking UART1 console output;
- `Clock`: the H747 tick source anchored to the deterministic evidence epoch on
  its first read;
- `BlockDevice`: four volatile 512-byte RAM blocks.

The RAM-backed BlockDevice is intentional. This slice proves capability
resolution and application semantics without making QSPI/eMMC availability or
write safety part of the MVP.

The surrounding H747 `init.graph` orders console and app initialization. It is
not the Capability Binding model. The portable `Requirement`, `Provision`,
`Binding`, `ProfileView`, and pre-start resolution failure remain the single
shared definitions in `mvp_composition.hpp`.

Build and capture from `Examples/project/h747-lab`:

```powershell
cmake --preset h747-lab-capability-mvp-debug
cmake --build --preset build-h747-lab-capability-mvp-debug -- -j1
.\tools\capture-capability-mvp-board-smoke.ps1
```

A passing board capture contains:

```text
charm-mvp: ok
[charm-capability-mvp-h747] positive=ok timestamp=424242 checksum=0x49b880f0
[charm-capability-mvp-h747] missing=missing_binding start_count=0
[charm-capability-mvp-h747] ok
```

The target being buildable is necessary but not sufficient evidence. The real
board UART log now passes both the capture script and
`verify_cross_environment_evidence.ps1`, matching Host/QEMU timestamp and
checksum evidence. This closes the MVP execution requirement without promoting
the local contract projections into public Core APIs.
