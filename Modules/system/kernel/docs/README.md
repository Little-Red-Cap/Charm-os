# Kernel 文档入口

本目录主要收纳 `Modules/system/kernel/` 的阶段性设计、草案和专题说明。

这里的材料大多不是当前仓库级“现行总契约”的第一入口。  
如果你在看当前系统主线、最小内核、平台边界或 POSIX 路线，优先先回到：

- [`../../../../docs/system/README.md`](../../../../docs/system/README.md)
- [`../../../../docs/architecture_overview.md`](../../../../docs/architecture_overview.md)

## 先怎么读

### 想理解当前内核骨架与配置组合

先读：

- [`kernel_config_profiles.md`](kernel_config_profiles.md)
- [`event_queue_backends.md`](event_queue_backends.md)

### 想看更早期的结构草案

读：

- [`kernel_util_structure_draft.md`](kernel_util_structure_draft.md)

注意：

- 这是一份历史结构草案，适合帮助理解演化方向；
- 不应直接替代当前 `docs/system/*` 下的契约与入口文档。

### 想看 M1 / M2 / M3 分阶段材料

- M1：
  [`m1_sync_spec.md`](m1_sync_spec.md)、[`m1_tests.md`](m1_tests.md)
- M2：
  [`m2_thread_spec.md`](m2_thread_spec.md)、[`m2_api_freeze.md`](m2_api_freeze.md)
- M3：
  [`m3_observability_plan.md`](m3_observability_plan.md)

这些文档更偏阶段性设计与冻结点，不默认表达当前仓库的全部系统事实。

## 使用提醒

- 需要“当前系统怎么装配、怎么启动、哪些边界是现行约束”，请先回到 `docs/system/`。
- 需要“内核这条线早期是怎么设计出来的”，再回到本目录。
- 如果本目录文档和当前代码或 `docs/system/*` 冲突，优先以更上位入口和当前代码为准。
