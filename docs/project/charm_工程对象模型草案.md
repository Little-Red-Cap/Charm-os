# Charm 工程对象模型摘要

> **文档状态：`exploration`（未冻结）**

本页只保存项目组合压力与候选名词的当前裁决。它不能覆盖 Constitution、现行契约或 CMake 事实。

## 真实问题

复杂项目需要表达：artifact 跑在哪个 execution environment、使用哪块 board/BSP、选择哪组 profile、
由哪些 target/preset/workflow 构建与验证。这些是项目组合输入，不要求建立一个覆盖全仓的对象注册表。

## 当前裁决

| 候选名词 | 当前角色 |
|---|---|
| Product、Board/BSP、Workspace | `Project Fact`，由具体项目拥有 |
| Execution Environment | `Stable Boundary`，区分 Host、QEMU、真实板等承载环境 |
| Profile | `Stable Boundary`，一组项目级组合选择，不定义 capability |
| Backend、Driver、Workflow | `Implementation / Tool`，不进入 Core 身份 |
| Platform | 含义过载；具体使用必须说明是 execution environment、BSP 事实还是 backend |
| Scenario、Variant | 可作局部项目命名，没有统一跨仓语义 |
| Bundle | 已删除的泛化组合名词；具体组合应由 target、profile 或专题 helper 说明 |
| Runtime | `Rejected / Deferred`；必须命名为具体 App、scheduler、OS 或协议 runtime |

Dependency scope 由真实 target link/import、工具依赖与项目 ownership 表达，不建立独立 Core 对象。

## 当前状态

仓库没有统一 `charm_add_product/platform/board/scenario/runtime/bundle` API，也不应仅为让 CMake 看起来
更高层就引入它们。当前 target、preset、source manifest 和项目目录是可核对事实。

构建与迁移取舍见
[`build_model_retained_notes.md`](../archive/project-proposals-v0/build_model_retained_notes.md)。

## 重新推进条件

先明确唯一消费方、最小输入/输出和失败语义，使用至少两个真实 target 或 execution environment 验证。
只有现有 target/preset/profile/BSP 约定无法表达需求时，才新增一个项目层对象；局部项目名不能直接
晋升为 Charm 公共词汇。
