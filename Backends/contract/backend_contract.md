# Charm Backend Contract

## 文档状态

- `status`: `contract candidate`
- `scope`: backend topology、capability binding、provider evidence 与已提取 capability slices
- `authority`: 本目录 contract headers 与 header/reference/system smokes

该 contract 独立于 `Modules/platform/`。现有 platform prototype 可以被 backend adapter 包装，但不能反向
定义 backend 词汇。

## 组合模型

```text
Application/Domain Requirement
  -> Profile Binding
  -> Provider Instance
  -> Provider Adapter
  -> Backend Resource
```

binding 只能指向 provider instance。provider type、adapter、transport、HAL handle、backend token、file path
和 endpoint 都不是 binding target。

## 词汇边界

| term | contract |
|---|---|
| backend | Charm 执行资源域；拥有 provider instances、adapters 和 OS/simulator/HAL dependencies |
| runtime domain | backend 内实际执行代码的 domain，如 host process、QEMU core、MCU core |
| capability | app/domain 可消费的语义契约 |
| requirement | consumer 对 capability role 的声明 |
| binding | profile 将 requirement 映射到 provider instance 的装配结果 |
| provider type | implementation family metadata，不是实例 identity |
| provider instance | profile 可选择的具体 provider identity |
| adapter | provider 内部机制转换，不进入 consumer dependency |
| endpoint | provider 发布的消费 surface，不等于 provider instance |
| BSP | real-board clocks/pinmux/memory/startup/vendor glue 和硬件 binding |
| target | 选择 toolchain/architecture/backend/board/profile 的 build leaf |
| HAL | adapter 下方的 vendor/controller interface，不是跨环境 Charm contract |

`platform` 是历史源码区域，`port` 只用于 early bring-up 等例外入口；二者都不是新的 capability boundary。

## Backend Evidence

backend 必须能投影：

```text
BackendIdentity
ProviderInstances
CapabilityExports
SelectedBindings
BackendFacts
ReadinessSummary
```

fact 必须包含 stable kind/name、required/optional、provided/missing/unknown 和来源。日志可以展示 evidence，
但日志文本不是 schema。missing 或 unknown required fact 不得产生 ready verdict。

backend identity/facts 不能伪造环境证据：Host、QEMU 和真实板各自证明自己的 execution domain；QEMU
console/timer/trap 不能替代 H747 外设，board build-only 也不能替代运行日志。

## Contract Headers

| header | ownership |
|---|---|
| `capability_topology.hpp` | requirement、provided token、provider instance、binding、context topology |
| `backend_evidence.hpp` | backend identity、exports、selected binding、facts 与 readiness |
| `console_output.hpp` | `ByteSink`、`TextSink`、`LineSource`、roles、transfer/status evidence |
| `block_storage.hpp` | `BlockDevice`、roles、published `BlockEndpoint`、status/evidence |
| `raster_display.hpp` | bounded read-only raster、pixel format、dirty/clipping/present result |

这些 header 仍是 `Backends/contract` candidate，不自动成为 `Modules/` public API。Store/FAT/ImageStore、
ResourcePack、ELF/ModuleX、window/texture/scaling、cache/frame scheduling 和 screenshot policy 属于上层或具体
backend，不进入这些 slices。

## Dependency Rules

- app/domain 只消费 capability role 和中性 surface，不依赖具体 backend/provider identity；
- `Modules/system` 不 import Host/QEMU/board implementation；
- concrete backend 可以依赖 contract 和自己的 OS/simulator/BSP/HAL adapter；
- backend implementation 之间不能互相依赖；
- legacy platform wrapper 必须保持单向且可移除。

## 验证

当前 gate：[`run-backends-v1-smoke.ps1`](../run-backends-v1-smoke.ps1)。它组合 contract header、Host/QEMU/
board reference 和 system provider smokes；各 target 与负例以脚本/CMake/source 为准，本文不复制清单。

该脚本当前为多个独立 source 创建多个 build tree，磁盘受限环境不要直接运行；应先提供受控 build root/
清理策略或逐个复用已有 fixture 输出。smoke 通过只证明 candidate contract 覆盖的局部语义。

## 非目标

- 不定义 scheduler、service locator、manifest/generator、YAML/DSL 或产品 boot policy；
- 不要求迁移全部 `Modules/platform`；
- 不让 Backend/BSP/target 进入 App ABI；
- 不把 reference provider 或 smoke metadata 当作生产 backend 完成证据。
