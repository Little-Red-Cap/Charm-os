# H747 Resident Launcher

`resident_launcher` is the first boot-facing resident App launcher prototype.
It is deliberately narrower than `dev_loader`: there are no download commands,
no App Store install commands, and no second App ABI.

Current v1 path:

```text
boot resident_launcher
  -> initialize raster display, encoder input, and eMMC
  -> enumerate /CHARM/APPS/*.ELF from the eMMC FAT filesystem
  -> choose with encoder detents and press to run
  -> file bytes -> AppImage(format=elf)
  -> staged AppImageSource -> ELF loader -> AppRuntime -> CharmAppApi
```

The launcher uses SDRAM as file/stage cache and the fixed D1 ELF run region at
`0x24070000..0x24080000`. That address must stay aligned with
`Examples/app_abi/elf_samples/app_elf.ld`.

The code is split along the resident platform boundary:

- `resident_launcher_domain.hpp`: host-testable catalog state, selection,
  run request, run record, and view model.
- `charm_app_fat_catalog.hpp`: read-only FAT32 catalog and file staging source.
- `resident_launcher.cpp`: H747 binding for eMMC block read, raster rendering,
  encoder input, SDRAM stage cache, D1 ELF run region, and `AppRuntime`.

Recommended provisioning flow:

```powershell
Examples/app_abi/elf_samples/build_resident_platform_artifacts.ps1 -Validate
Examples/project/h747-lab/tools/provision-resident-launcher-emmc-apps.ps1 `
  -DriveRoot <usb-msc-drive-root> `
  -ArtifactManifest Examples/app_abi/elf_samples/out/artifact_manifest.json
```

The provision script copies only ELF artifacts into `CHARM/APPS`; ModuleX is
intentionally rejected by Launcher v1.

Smoke/evidence helpers:

```powershell
Examples/project/h747-lab/tools/provision-resident-launcher-emmc-apps.ps1 -SelfTest
Examples/project/h747-lab/tools/capture-resident-launcher-smoke.ps1 -SelfTest
Examples/project/h747-lab/tools/capture-resident-launcher-smoke.ps1 -DryRun
```

v1 scope:

- ELF only; ModuleX can join later through the same catalog shape.
- eMMC FAT is read-only from the launcher.
- App exit does not restore a full launcher session; reset is the return path.
- UI is an engineering list screen, not the Player UI stack.
