# 文档索引

本页是 `docs/` 的总入口。它负责让人快速找到当前最可信的入口，而不是把所有阶段材料都摊在首页。

如果你是第一次进入仓库，先读：

1. [`overview.md`](overview.md)
2. [`architecture_overview.md`](architecture_overview.md)
3. `README.md`（当前页面）
4. 再按任务进入 `capability / system / board / io` 等专题入口

## 先分清哪类文档最值得信

优先级从高到低：

1. `README.md` / `*_overview.md`
2. `*_contract.md`
3. `*_plan.md` / `*_roadmap.md` / `*_draft.md`
4. `*_review.md` / `*_summary.md` / `*_v0.md`
5. `*_tasklist.md` / `*_checklist.md`
6. `reference/*` / `generated/*`

## 最常用入口

- [`capability_map.md`](capability_map.md) - 先回答“Charm 已经有哪些能力、我该优先用哪个”
- [`system/README.md`](system/README.md) - 系统装配、启动边界、system coordination
- [`architecture/README.md`](architecture/README.md) - 架构边界、依赖红线、能力归属
- [`board/README.md`](board/README.md) - 板级 bring-up
- [`io/README.md`](io/README.md) - IO / channel / reactor / registry / out / shell
- [`project/README.md`](project/README.md) - 开始改代码前的协作规范
- [`agent/README.md`](agent/README.md) - AI / Agent 协作入口

## 我现在该看什么

| 你现在想做什么 | 先看什么 |
| --- | --- |
| 不知道 Charm 有没有这个能力 | [`capability_map.md`](capability_map.md) |
| 在做系统装配或启动边界 | [`system/README.md`](system/README.md) |
| 在做 ARMv7-A / QEMU / 最小内核 | [`system/README.md`](system/README.md) |
| 在做板级 bring-up | [`board/README.md`](board/README.md) |
| 在做 IO / 输出 / shell | [`io/README.md`](io/README.md) |
| 在做架构判断或能力归属 | [`architecture/README.md`](architecture/README.md) |
| 在准备开始改代码 | [`project/README.md`](project/README.md) |
| 在看历史阶段证据链 | [`archive/system-compiler-front-page-v0/README.md`](archive/system-compiler-front-page-v0/README.md) |

## 当前保留的系统编译器 / explain surface 入口

- [`architecture/system_compiler_roadmap.md`](architecture/system_compiler_roadmap.md)
- [`architecture/system_compiler_vocabulary_v0.md`](architecture/system_compiler_vocabulary_v0.md)
- [`system/artifact_report_v0.md`](system/artifact_report_v0.md)
- [`system/explain_surface_v0.md`](system/explain_surface_v0.md)
- [`system/resource_contract_v0.md`](system/resource_contract_v0.md)
- [`system/opening_judgment_corridor_witness_taxonomy_v0.md`](system/opening_judgment_corridor_witness_taxonomy_v0.md)

这些是当前仍然保留价值的系统编译器与 explain surface 入口；它们不等于 front-page / opening-flow / biography / world / witness 阶段材料清单。

## 不要怎么读

- 不要把 `docs/README.md` 当成所有阶段材料目录。
- 不要把 `*_v0.md` 自动当成当前契约。
- 不要把 `reference/*` 或 `generated/*` 当作一手入口。
- 不要先翻归档目录再建立整体认知。

## 历史材料

早期 `system compiler front-page / opening-flow / biography / world / witness` 阶段材料已归档到：

- [`archive/system-compiler-front-page-v0/README.md`](archive/system-compiler-front-page-v0/README.md)
