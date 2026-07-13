# Review Workflow

本文件定义 Charm 项目 review 类任务的标准执行路径。
目标：让审查过程可重复、可追踪、不依赖临场发挥。

相关文件：
- `../rules/README.md`
- `../rules/charm-architecture.md`
- `../rules/embedded-modern-cpp.md`
- `../rules/collaboration.md`
- `../skills/code-review/SKILL.md`
- `../skills/code-review/checklist.md`
- `../templates/review-output.md`

---

## 1. 适用范围

适用于：
- review 代码/模块/PR
- 判断实现是否符合 Charm 约束
- 判断改动是否存在阻断/重要/优化问题

不适用于：
- 从零生成代码
- 讨论能力归属（应切换 architect-review）
- 纯协作方式对齐

---

## 2. 总体原则

- 先判对象类型，再展开检查
- 先查硬性违规，再查设计退化，再看优化空间
- 先使用项目规则，再使用经验判断
- 信息不足时明确说明

---

## 3. 执行顺序

### 第一步：识别 review 对象
- 单函数 / 单模块 / 适配层 / PR / 架构级改动

若问题属于能力归属或装配，应切换到 `architect-review`。

### 第二步：加载规则
优先读取：
1. `../rules/charm-architecture.md`
2. `../rules/embedded-modern-cpp.md`

如需协作对齐再补：
3. `../rules/collaboration.md`

### 第三步：加载技能
- `../skills/code-review/SKILL.md`

### 第四步：使用 checklist
- `../skills/code-review/checklist.md`

### 第五步：判断信息是否不足
若缺少关键上下文，明确列出需要补充的内容。

### 第六步：套用输出模板
- `../templates/review-output.md`

---

## 4. 分流规则

### 切换到 architect-review 的条件
- 能力归属/分层/装配路径问题
- init.graph、target wiring 或装配边界选择
- 系统级边界判断

### 补充 collaboration 的条件
- 方案分歧明显
- 设计未对齐
- 需要用户偏好才能推进

---

## 5. 输出风格要求

- 直接、清晰、高标准
- 有依据，不做空泛评价
- 结论可执行
