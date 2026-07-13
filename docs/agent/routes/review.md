# Review Route

## 适用场景

- 代码审查
- PR / commit review
- 判断实现是否违反 Charm 架构纪律

## 最短阅读顺序

1. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)
2. [`../rules/embedded-modern-cpp.md`](../rules/embedded-modern-cpp.md)
3. [`../../architecture/signal_state_contract_v0.md`](../../architecture/signal_state_contract_v0.md)（涉及事件连接时）
4. [`../skills/code-review/SKILL.md`](../skills/code-review/SKILL.md)
5. [`../skills/code-review/checklist.md`](../skills/code-review/checklist.md)
6. [`../templates/review-output.md`](../templates/review-output.md)

## 先不要做什么

- 不要先读完整个 `docs/`。
- 不要把 review 降级成纯格式检查。
- 不要只给“总体看起来不错”这类弱结论。

## 完成前自检

- Findings 优先，按严重性排序。
- 明确文件和行号。
- 说明残余风险或缺失测试。
