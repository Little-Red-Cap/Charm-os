# H747 Resident Launcher

## 文档状态

- `status`: `supporting`
- `scope`: eMMC FAT catalog 到 AppRuntime 的 boot-facing ELF launcher
- `source`: [`resident_launcher.cpp`](resident_launcher.cpp)、[`resident_launcher_domain.hpp`](resident_launcher_domain.hpp)

`resident_launcher` 不提供 download、Store install 或第二 App ABI：

```text
boot -> display/input/eMMC
-> enumerate /CHARM/APPS/*.ELF
-> encoder select/run
-> FAT file -> AppImage(elf) -> staged source -> ELF loader -> AppRuntime
```

文件/stage cache 位于 SDRAM，D1 execute region 固定为 `0x24070000..0x24080000`，并与
[`app_elf.ld`](../../../../app_abi/elf_samples/app_elf.ld) 对齐。

## Ownership

| 入口 | 责任 |
|---|---|
| [`resident_launcher_domain.hpp`](resident_launcher_domain.hpp) | catalog state、selection、run request/record 与 view model |
| [`charm_app_fat_catalog.hpp`](../../../../app_abi/charm_app_fat_catalog.hpp) | read-only FAT32 catalog 与 file staging |
| [`resident_launcher.cpp`](resident_launcher.cpp) | eMMC、raster/encoder、SDRAM/D1 region 与 AppRuntime H747 binding |

## Provision 与验证

| 任务 | 入口 |
|---|---|
| 生成 canonical ELF artifact | [`build_resident_platform_artifacts.ps1`](../../../../app_abi/elf_samples/build_resident_platform_artifacts.ps1) `-Validate` |
| 复制到 `CHARM/APPS` | [`provision-resident-launcher-emmc-apps.ps1`](../../tools/provision-resident-launcher-emmc-apps.ps1) |
| capture/self-test/dry-run | [`capture-resident-launcher-smoke.ps1`](../../tools/capture-resident-launcher-smoke.ps1) |

Provision 只接受 manifest 中的 ELF，拒绝 ModuleX。参数、token 与当前状态由脚本维护。

## v1 边界

- eMMC FAT 对 launcher 只读；
- App exit 后以 reset 返回，不恢复完整 launcher session；
- UI 只是工程 list screen，不属于 Player UI stack；
- ModuleX 只有在复用同一 catalog/AppRuntime 边界后才能加入。
