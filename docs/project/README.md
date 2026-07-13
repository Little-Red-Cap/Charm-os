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
| 当前认领与 backlog | [`tracking/README.md`](tracking/README.md) |
| 低层实现例外 | [`escape_hatches.md`](escape_hatches.md) |
| PowerShell / UTF-8 | [`Powershell设置utf8.md`](tooling/Powershell设置utf8.md) |
| CMake / preset / 构建目录 | [`build route`](../agent/routes/build.md) |

系统与架构问题优先进入 [`architecture_overview.md`](../architecture_overview.md) 和
[`system/README.md`](../system/README.md)。

## 提案与历史

[`charm_工程对象模型草案.md`](charm_工程对象模型草案.md) 只保留项目组合候选词的未冻结裁决，
不是现行系统契约。其它未实施项目提案只保留归档正文：

- Bundle、工程对象/变体、构建升级和 USB 装配：
  [`project-proposals-v0`](../archive/project-proposals-v0/README.md)
- C++ 实践、协作宣言和 tracking 快照：
  [`project-guidance-and-tracking-v0`](../archive/project-guidance-and-tracking-v0/README.md)

## 规则

- 需要当前事实时，先查源码、CMake target/preset 和当次验证结果。
- 提案中的对象名不因被多篇文档引用而成为公共模型。
- 新项目规则进入 `standards/`；阶段状态进入 `tracking/`；历史材料进入 `archive/`。
