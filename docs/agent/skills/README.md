# Skills 路由

> status: `supporting`
>
> Skill 描述某类任务的执行方法；长期约束由 [`../rules/`](../rules/README.md) 定义。任务应先从
> 根 [`AGENTS.md`](../../../AGENTS.md) 和对应 [`route card`](../routes/README.md) 进入，不要预加载全部 skill。

## 当前入口

| 任务 | Skill |
|---|---|
| 代码审查 | [`code-review/SKILL.md`](code-review/SKILL.md) |
| 代码生成与模块骨架 | [`codegen/SKILL.md`](codegen/SKILL.md) |
| 架构评审与能力归属 | [`architect-review/SKILL.md`](architect-review/SKILL.md) |
| init.graph 与板级装配 | [`charm-init-graph/SKILL.md`](charm-init-graph/SKILL.md) |
| Channel/Reactor/Registry | [`charm-io-contracts/SKILL.md`](charm-io-contracts/SKILL.md) |
| Capability 实现索引 | [`charm-capability-map/SKILL.md`](charm-capability-map/SKILL.md) |
| Block device 与 VFS 挂载 | [`charm-block-device/SKILL.md`](charm-block-device/SKILL.md) |
| 文档路由与 dead link | [`charm-docs-minimal/SKILL.md`](charm-docs-minimal/SKILL.md) |
| UTF-8 与乱码修复 | [`charm-docs-utf8/SKILL.md`](charm-docs-utf8/SKILL.md) |
| CMake 与构建接线 | [`charm-cmake/SKILL.md`](charm-cmake/SKILL.md) |

## 维护规则

- Skill 依赖 rules，不得覆盖或复制 rules 正文。
- Skill 只描述任务流程和检查重点，不定义 Charm Core、接口契约或项目状态。
- `checklist.md`、`examples.md`、`templates/`、`schemas/` 和 `scripts/` 仅在该 skill 有独立需要时添加。
