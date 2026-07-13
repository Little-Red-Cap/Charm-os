# Skills 导读

本目录存放 Charm 项目中面向具体任务的技能文件。

技能不是长期稳定的全局规则，而是 AI 在执行某类任务时的工作说明。  
技能应建立在 `docs/agent/rules/` 下的规则之上，不能脱离规则单独使用。

## 与 rules 的区别

### rules
rules 用于定义长期有效的边界，例如：

- 如何协作
- 现代嵌入式 C++ 的技术立场
- Charm 项目的工程纪律

rules 回答的是：

> “在 Charm 中，AI 应该遵守什么？”

### skills
skills 用于定义某类任务的执行方式，例如：

- 代码审查
- 代码生成
- 架构评审

skills 回答的是：

> “在 Charm 中，AI 遇到这个任务时，应该怎么做？”

## 当前技能

### `code-review/`
用于 Charm 项目中的代码审查任务。

适用：
- review 某段代码
- review 某个 PR
- 判断某个实现是否违反 Charm 约束
- 判断某段实现是否回退到不符合项目立场的传统写法

依赖规则：
- `rules/charm-architecture.md`
- `rules/embedded-modern-cpp.md`
- 必要时参考 `rules/collaboration.md`

### `codegen/`
用于 Charm 项目中的代码生成与模块骨架设计。

依赖规则：
- `rules/charm-architecture.md`
- `rules/embedded-modern-cpp.md`
- 必要时参考 `rules/collaboration.md`

### `architect-review/`
用于 Charm 项目中的架构评审与能力归属判断。

依赖规则：
- `rules/charm-architecture.md`
- `rules/embedded-modern-cpp.md`
- 必要时参考 `rules/collaboration.md`

## 读取原则

### 当任务是 review
优先读取：

1. `rules/charm-architecture.md`
2. `rules/embedded-modern-cpp.md`
3. `skills/code-review/SKILL.md`
4. `skills/code-review/checklist.md`

### 当任务是生成代码
不应直接复用 review 结论，应改为读取 codegen skill。

### 当任务只是讨论协作方式
不应优先进入 skills，应先回到 `rules/collaboration.md`

## 技能编写约定

每个技能目录建议包含：

- `SKILL.md`
  - 定义该技能的用途、适用场景、工作流程、输出要求

- `checklist.md`
  - 定义该技能的检查清单
  - 适合 review、审核、验收类任务

技能目录可以包含 `examples.md`、`templates/`、`schemas/` 或 `scripts/`，但只有在该技能确实需要
独立示例、结构化输入或自动检查时才添加。

## 总体原则

- 技能必须依赖规则
- 技能不得覆盖规则
- 技能只组织任务流程，不重新定义项目哲学与架构边界
- 技能的目标是提高任务执行一致性，而不是重复所有规则正文


# 技能清单

当前可用 skill（可复制到 `.codex/skills`）：

- `code-review`
- `codegen`
- `architect-review`
- `charm-init-graph`
- `charm-io-contracts`
- `charm-capability-map`
- `charm-block-device`
- `charm-docs-minimal`
- `charm-docs-utf8`
- `charm-cmake`
