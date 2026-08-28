# 项目文档入口

## 文档状态

- `status`: `supporting`
- `scope`: 项目规范、协作、推进材料与项目级提案路由
- `authority`: [`../../AGENTS.md`](../../AGENTS.md)

本目录不是系统行为的事实源。项目文档与现行源码、构建入口或系统契约冲突时，以后者为准。

## 按任务进入

| 任务 | 入口 |
|---|---|
| 修改或审查代码 | [`standards/README.md`](standards/README.md)、[`codegen route`](../agent/routes/codegen.md)、[`review route`](../agent/routes/review.md) |
| 协作约定 | [`collaboration.md`](../agent/rules/collaboration.md)、[`Agent README`](../agent/README.md) |
| 低层实现例外 | [`escape_hatches.md`](escape_hatches.md) |
| PowerShell / UTF-8 | [`Powershell设置utf8.md`](tooling/Powershell设置utf8.md) |
| CMake / preset / 构建目录 | [`build route`](../agent/routes/build.md) |
| OnlyCore 提纯 | [`only_core_distillation_sop.md`](only_core_distillation_sop.md) |
| OnlyCore 当前清单 | [`only_core_distillation_manifest.md`](only_core_distillation_manifest.md) |

系统与架构问题优先进入 [`architecture_overview.md`](../architecture_overview.md) 和
[`architecture/charm_core_contract.md`](../architecture/charm_core_contract.md)。OnlyCore 当前实现入口
见 [`only_core_distillation_sop.md`](only_core_distillation_sop.md)。

## 规则

- 需要当前事实时，先查源码、CMake target/preset 和当次验证结果。
- 提案中的对象名不因被多篇文档引用而成为公共模型。
- 新项目规则进入 `standards/`；历史材料只进入独立快照分支或 Git 历史。
