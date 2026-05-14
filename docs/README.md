# 文档入口

本页是 `docs/` 的当前路由入口。它只负责回答“先看哪里”，不再逐篇列出历史阶段材料。

如果你是第一次进入仓库，先读：

1. [`overview.md`](overview.md)
2. [`architecture_overview.md`](architecture_overview.md)
3. 本页
4. 再按任务进入对应专题入口

## 当前入口

| 目的 | 先看 |
|---|---|
| 找现有能力、默认路径、示例 | [`capability_map.md`](capability_map.md) |
| 看系统装配、启动、minimal-kernel、POSIX | [`system/README.md`](system/README.md) |
| 看 minimal-kernel runtime ledger fact contract | [`system/minimal_kernel_runtime_ledger_fact_contract_v0.md`](system/minimal_kernel_runtime_ledger_fact_contract_v0.md) |
| 看脚本面收敛与 evidence harness 治理 | [`system/script_surface_reduction_governance_v0.md`](system/script_surface_reduction_governance_v0.md) |
| 看架构边界、驱动模型、能力归属 | [`architecture/README.md`](architecture/README.md) |
| 看 IO / Channel / Reactor / Registry | [`io/README.md`](io/README.md) |
| 看存储与 block device | [`storage/README.md`](storage/README.md) |
| 看音频、USB、板级资料 | [`audio/README.md`](audio/README.md)、[`usb/README.md`](usb/README.md)、[`board/README.md`](board/README.md) |
| 开始改代码或查工程约定 | [`project/README.md`](project/README.md) |
| Agent 任务路由 | [`agent/routes/README.md`](agent/routes/README.md) |
| 文档维护、归档、入口清理 | [`documentation_maintenance.md`](documentation_maintenance.md) |

## 信任顺序

遇到同一主题下文档很多时，按这个顺序判断：

1. `README.md` / `*_overview.md`：入口与主题边界。
2. `*_contract.md`：现行行为、接口与边界约束。
3. `*_plan.md` / `*_roadmap.md` / `*_draft.md`：方向、迁移或设计草案。
4. `*_review.md` / `*_summary.md` / `*_v0.md`：阶段快照，需要结合当前代码阅读。
5. `*_tasklist.md` / `*_checklist.md`：推进、验收和排期，不默认充当主题首页。
6. `reference/*` / `generated/*`：参考材料或机器生成结果，不是默认一手入口。

## 系统编译器 / explain surface 当前入口

system compiler 的 front-page、opening-flow、witness、biography、world compare 等材料仍可追溯，但不作为默认首读路线。

当前只保留这些上位入口：

- [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)
- [`architecture/system_compiler_vocabulary_v0.md`](architecture/system_compiler_vocabulary_v0.md)
- [`system/artifact_report_v0.md`](system/artifact_report_v0.md)
- [`system/explain_surface_v0.md`](system/explain_surface_v0.md)
- [`system/resource_contract_v0.md`](system/resource_contract_v0.md)
- [`system/bringup_evidence_pipeline_v0.md`](system/bringup_evidence_pipeline_v0.md)
- [`system/opening_judgment_corridor_witness_taxonomy_v0.md`](system/opening_judgment_corridor_witness_taxonomy_v0.md)

不要从阶段材料反推当前默认入口；先回到本页和对应目录 `README.md`。

## 历史材料

早期 `system compiler front-page / opening-flow / biography / world / witness` 阶段材料已归档到：

- [`archive/system-compiler-front-page-v0/README.md`](archive/system-compiler-front-page-v0/README.md)

## 不要怎么读

- 不要把 `docs/README.md` 当成所有阶段材料目录。
- 不要把 `*_v0.md` 自动当成当前契约。
- 不要把 `reference/*` 或 `generated/*` 当作一手入口。
- 不要先翻归档目录再建立整体认知。
