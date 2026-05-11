# soa_demo

This is the Vivid SoA table/tree internal regression entrypoint. It verifies demo-local evidence only and does not enter the player or Evidence Lab mainline.

## Run

```powershell
.\Examples\ui\vivid\soa_demo\soa_ci.ps1 -Mode ci
.\Examples\ui\vivid\soa_demo\soa_ci.ps1 -Mode dump
.\Examples\ui\vivid\soa_demo\soa_ci.ps1 -Mode replay -ReplayPath artifacts/soa_ci/soa_ci.vcmd
```

By default it configures a dedicated build directory under `Examples/ui/vivid/soa_demo/cmake-build-soa-ci` and enables `CHARM_VIVID_SOA_TRACE_INPUT=ON`.

## Boundaries

- This is still an internal regression gate, not part of `vivid_evidence_lab_manifest_v0.md`.
- `soa_demo/main.cpp` keeps a stable stdout contract.
- `CHARM_VIVID_SOA_TRACE_INPUT` is only enabled in the dedicated CI configuration.
