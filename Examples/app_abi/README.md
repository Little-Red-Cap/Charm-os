# Charm App ABI 原型

## 边界

本目录定义 resident App 的第一代 C ABI 原型：

```c
extern "C" int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

App 只消费 `CharmAppApi` capability table。它不是 C++ ABI、service locator、manifest language
或 POSIX `main(argc, argv, envp)`。当前 capability 包括 console、time、display、input、optional
storage、reserved AFE 与 app exit。

主链固定为：

```text
ImageSource/ImageStore -> AppImage -> loader -> AppRuntime -> CharmAppApi -> charm_app_main
```

ELF 与 ModuleX 是 image format，不定义第二套 App model。架构边界见
[`resident_image_platform_v1_contract.md`](../../docs/architecture/resident_image_platform_v1_contract.md)。

## Runtime

[`charm_app_runtime.hpp`](charm_app_runtime.hpp) 负责 image lookup、loader callback、API 校验、argv、
execute-ready preparation、entry 调用和 `lookup/load/abi/argv/start/exit` 诊断。`prepare()` 停在
start 前，不调用目标代码；`run()` 才执行 entry。

[`charm_app_staged_runtime.hpp`](charm_app_staged_runtime.hpp) 将一个已 staged `AppImage` 和 loader
包装为单 image source。received、QSPI、eMMC 和 FAT catalog 都必须在该边界前完成读取，不能各自
复制 AppRuntime 或 entry ABI。

## Image format

### ELF

[`charm_app_elf_probe.hpp`](charm_app_elf_probe.hpp) 校验 ELF32 `PT_LOAD`、entry offset、load span、
segment、alignment 和 load buffer capacity。probe/load 不拥有 image bytes 或 execute region；平台
负责区域容量、可执行性和 cache preparation。

### ModuleX

[`charm_app_modulex_loader.hpp`](charm_app_modulex_loader.hpp) 解析 global `charm_app_main`，执行
dependency/external binding 与 `abs_addr/rel32` relocation，并产出相同 `CharmAppMainFn`。App v1
拒绝 non-zero BSS 和 XIP flags。CM7 callable address 由板端 materializer 设置 Thumb bit；host
function pointer 不修改。

ModuleX 的原生 C++ struct 含 `uintptr_t`；跨架构 artifact 使用 pack/inspect 工具的显式 32-bit
wire layout，不能按 host `sizeof(modulex::Symbol)` 打包。格式细节见
[`ModuleX_格式草案.md`](../../Modules/system/modulex/ModuleX_格式草案.md)。

## Image source 与 Store

| 文件 | 职责 |
|---|---|
| [`charm_app_received_image.hpp`](charm_app_received_image.hpp) | 将 verified received bytes stage 为 `AppImage` |
| [`charm_app_fat_catalog.hpp`](charm_app_fat_catalog.hpp) | 从只读 FAT32 `/CHARM/APPS/*.ELF` 读取 named image |
| [`charm_app_store.hpp`](charm_app_store.hpp) | Store v1 header/entry、reader、builder 与 staging |
| [`charm_app_store_install.hpp`](charm_app_store_install.hpp) | erase/write/readback verify 的 flash-like install 语义 |

Store v1 是 `header + entries + payload`，不是 filesystem、package manager 或 update slot。entry
`flags & 0xF` 中 `0=ELF`、`1=ModuleX`；旧 zero flags 仍解释为 ELF。QSPI/eMMC backend 只提供
media/read/write，不定义第二套 Store 或 App entry。

received/stage cache 可以位于 SDRAM；execute region 属于 runtime domain。内存放置不进入 App ABI。

## Runtime domain

Host、H747 CM7、QEMU MPS2 和未来 remote/core domain 可以使用不同 linker address、load region 和
capability backend，但 App-visible ABI 不变。

[`resident_elf_qemu_smoke`](../system/resident_elf_qemu_smoke/README.md) 只证明 QEMU Cortex-M7 上的
ELF loader、AppRuntime 与 capability 语义，不证明 H747 USB、QSPI、eMMC、SDRAM、HAL 或 cache。
真实板证据由 H747 resident runtime 脚本维护。

## Artifact 与证据入口

1. [`build_resident_platform_artifacts.ps1`](elf_samples/build_resident_platform_artifacts.ps1) 生成 ELF、
   ModuleX、mixed Store、packetstream 与 `artifact_manifest.json`。
2. [`resident-platform-inspect`](../system/resident_platform_inspect_tool/) 离板检查 manifest、CRC、
   packetstream、Store、ELF probe 与 ModuleX layout。
3. [`capture-resident-platform-evidence-bundle.ps1`](../project/h747-lab/tools/capture-resident-platform-evidence-bundle.ps1)
   默认运行 artifacts、inspect、host smokes 和 H747 build-only；`-BoardMatrix` 才占用板子。

`artifact_manifest.json` 只是 host/CI 证据索引，不写入 Store，也不是产品 manifest。

## 验证

Host fixture 从 [`Examples/system`](../system/README.md) 的 `app_abi_*`、`dev_loader_*` 与 `resident_*`
目录进入，分别覆盖 runtime/ABI、ELF/ModuleX、Store 和 artifact。target、负例与断言以各目录
CMake/source 为准；板级证据仍由上面的 H747 evidence bundle 维护。

[`player_min_core.h`](player_min_core.h) 是 capability/ABI 压力样本，不定义 Player UI 或平台 backend。
