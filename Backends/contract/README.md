# Backend Contract 入口

## 文档状态

- `status`: `supporting`
- `scope`: backend contract source、headers 与验证路由
- `authority`: [`backend_contract.md`](backend_contract.md)

## 入口

| 目标 | 文件 |
|---|---|
| Core relations | [`relations.hpp`](../../Modules/core/capability/relations.hpp) |
| common evidence/facts | [`backend_evidence.hpp`](backend_evidence.hpp) |
| console/output | [`console_output.hpp`](console_output.hpp) |
| block/storage | [`block_storage.hpp`](block_storage.hpp) |
| raster/display | [`raster_display.hpp`](raster_display.hpp) |

contract candidate 不依赖 `Modules/platform/`，不定义 runtime framework、scheduler、service locator、manifest
或 generator。domain-specific counters、geometry、trap/IRQ 和 board probe fields 留在对应 backend/evidence。

验证入口是 [`../run-backends-v1-smoke.ps1`](../run-backends-v1-smoke.ps1)。脚本、header smoke 和 reference
smoke 是当前覆盖事实源，README 不复制 target 清单或完成状态。
