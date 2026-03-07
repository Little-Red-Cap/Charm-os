# Codegen Workflow

本文件定义 Charm 项目代码生成类任务的标准执行路径。
目标：保证生成过程“先建模、后实现”，且符合项目约束。

相关文件：
- `../rules/collaboration.md`
- `../rules/embedded-modern-cpp.md`
- `../rules/charm-architecture.md`
- `../skills/codegen/SKILL.md`
- `../templates/codegen-output.md`

---

## 1. 适用范围

适用于：
- 新增模块/接口
- 补全实现骨架
- 将设计讨论落地

不适用于：
- 代码审查
- 架构归属判断（切换 architect-review）

---

## 2. 执行顺序

### 第一步：判断需求清晰度
- 需求清晰 → 继续
- 关键歧义 → 先讨论方案

### 第二步：加载规则
- `../rules/embedded-modern-cpp.md`
- `../rules/charm-architecture.md`
- 如需对齐协作 → `../rules/collaboration.md`

### 第三步：加载技能
- `../skills/codegen/SKILL.md`

### 第四步：建模再实现
- 明确 domain、核心类型、边界与装配路径
- 再生成代码骨架
- 最后补齐实现细节

### 第五步：套用输出模板
- `../templates/codegen-output.md`

---

## 3. 输出风格

- 结构清晰
- 先设计后代码
- 明确风险与未决点
