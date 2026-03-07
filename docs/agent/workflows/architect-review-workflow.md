# Architect Review Workflow

本文件定义 Charm 项目架构评审类任务的标准执行路径。
目标：以结构与边界优先，保证判断可长期演进。

相关文件：
- `../rules/charm-architecture.md`
- `../rules/embedded-modern-cpp.md`
- `../rules/collaboration.md`
- `../skills/architect-review/SKILL.md`
- `../templates/architect-review-output.md`

---

## 1. 适用范围

适用于：
- 能力归属/分层判断
- 装配路径与初始化纪律评审
- 系统级边界与演进方向评估

不适用于：
- 局部实现 review（切换 code-review）
- 纯协作方式对齐

---

## 2. 执行顺序

### 第一步：识别问题类型
- 能力归属 / 分层 / 装配 / 边界 / 演进

### 第二步：加载规则
- `../rules/charm-architecture.md`
- `../rules/embedded-modern-cpp.md`
- 如需对齐讨论 → `../rules/collaboration.md`

### 第三步：加载技能
- `../skills/architect-review/SKILL.md`

### 第四步：输出建议
- 明确推荐方案与边界
- 指出不推荐方案及风险
- 给出可落地的装配路径

### 第五步：套用模板
- `../templates/architect-review-output.md`

---

## 3. 输出风格

- 结构化
- 有依据
- 可落地
