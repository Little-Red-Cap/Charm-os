# Review Route

## 文档状态

- `status`: `supporting`
- `scope`: 代码、PR 与 commit review 路由

## 最短路径

1. [架构规则](../rules/charm-architecture.md)
2. [嵌入式 C++ 规则](../rules/embedded-modern-cpp.md)
3. [Code review skill](../skills/code-review/SKILL.md)
4. [Checklist](../skills/code-review/checklist.md)
5. [输出模板](../templates/review-output.md)

涉及 signal/state 时补读 [对应契约](../../architecture/signal_state_contract_v0.md)。输出以按严重性排序、
带文件行号的 findings 为主，并说明测试缺口和残余风险。
