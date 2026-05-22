# 文档总路由

本页是 `docs/` 的默认入口。

它不再按主题平铺所有材料，而是先回答：

- 你这次想理解哪条战线
- 你要看的是共同语义、真实压力、板级落地，还是维护态子系统
- 哪些入口是官方入口，哪些只是深层追溯入口

如果你还没建立整体认知，先读：

1. [`../README.md`](../README.md)
2. [`repo_governance.md`](repo_governance.md)
3. [`current_tracks_index.md`](current_tracks_index.md)

## 当前默认阅读意图

### 我想理解 Charm 的共同语义面

先读：

1. [`overview.md`](overview.md)
2. [`architecture_overview.md`](architecture_overview.md)
3. [`capability_map.md`](capability_map.md)
4. [`system/init_graph_contract.md`](system/init_graph_contract.md)

这条路径对应：

- `track_kind`: `substrate`
- `track_status`: `active`

### 我想理解当前的方法论探索

先读：

1. [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)
2. [`architecture/system_compiler_vocabulary_v0.md`](architecture/system_compiler_vocabulary_v0.md)
3. [`system/artifact_report_v0.md`](system/artifact_report_v0.md)
4. [`system/explain_surface_v0.md`](system/explain_surface_v0.md)

这条路径对应：

- `track_kind`: `theory`
- `track_status`: `exploring`

### 我想看真实产品压力线

先读：

1. [`../Examples/project/player/README.md`](../Examples/project/player/README.md)
2. [`../Examples/project/player/ARCHITECTURE_CONVERGENCE.md`](../Examples/project/player/ARCHITECTURE_CONVERGENCE.md)
3. [`ui/README.md`](ui/README.md)

这条路径对应：

- `track_kind`: `pressure`
- `track_status`: `active`

### 我想看板级 / SoC / runtime 落地线

先读：

1. [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md)
2. [`../targets/rk3506/README.md`](../targets/rk3506/README.md)
3. [`board/rk3506/README.md`](board/rk3506/README.md)

这条路径对应：

- `track_kind`: `landing`
- `track_status`: `active`

### 我想进入维护态子系统

先读：

1. [`system/posix_support_overview.md`](system/posix_support_overview.md)
2. [`system/posix_maintenance_mode_collaboration.md`](system/posix_maintenance_mode_collaboration.md)

这条路径对应：

- `track_kind`: `maintenance`
- `track_status`: `maintained`

## 两个治理页怎么配合

- [`repo_governance.md`](repo_governance.md)
  - 回答“这仓库现在同时在做什么”
- [`current_tracks_index.md`](current_tracks_index.md)
  - 回答“每条线的官方入口和深层入口分别是什么”

如果你只是想恢复上下文，优先读这两页，不要先潜入阶段材料。

## 当前入口矩阵

| 阅读意图 | 官方入口 |
|---|---|
| 共享能力底座 / 共同语义面 | [`overview.md`](overview.md)、[`architecture_overview.md`](architecture_overview.md)、[`capability_map.md`](capability_map.md) |
| system compiler / artifact / explain | [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)、[`system/artifact_report_v0.md`](system/artifact_report_v0.md) |
| Player / Vivid / 真实项目压力线 | [`../Examples/project/player/README.md`](../Examples/project/player/README.md)、[`ui/README.md`](ui/README.md) |
| RK3506 / minimal-kernel / ARMv7-A landing | [`system/minimal_kernel_runtime_evidence_bundle_contract.md`](system/minimal_kernel_runtime_evidence_bundle_contract.md)、[`../targets/rk3506/README.md`](../targets/rk3506/README.md) |
| POSIX 维护态 | [`system/posix_support_overview.md`](system/posix_support_overview.md)、[`system/posix_maintenance_mode_collaboration.md`](system/posix_maintenance_mode_collaboration.md) |
| 工程规范 / 构建 / 协作 | [`project/README.md`](project/README.md)、[`agent/routes/README.md`](agent/routes/README.md) |

## 深层材料如何进入

历史材料、`v0` 深水区、阶段链条、compare / witness / tasklist / checklist 仍保留，但不再默认混进首读路径。

推荐方式是：

1. 先从本页认战线
2. 再从 [`current_tracks_index.md`](current_tracks_index.md) 进对应深层入口
3. 最后按专题 README 或契约继续下钻

## 信任顺序

遇到同一主题下材料很多时，按这个顺序判断：

1. 仓库根目录 `AGENTS.md`
2. 根 README 与本页
3. 治理页与当前战线浅索引
4. 对应目录 `README.md` / `*_overview.md`
5. `*_contract.md`
6. `*_plan.md` / `*_roadmap.md` / `*_draft.md`
7. `*_v0.md` / `*_review.md` / `*_summary.md`
8. `*_tasklist.md` / `*_checklist.md`
9. `reference/*` / `generated/*` / `archive/*`

## 不要怎么读

- 不要把 `docs/README.md` 当成所有材料的平铺目录。
- 不要把任意 `v0` 自动当成当前唯一契约。
- 不要把 `archive/*`、`reference/*`、`generated/*` 当默认首读入口。
- 不要跳过治理页，直接从深层阶段材料反推仓库现状。
