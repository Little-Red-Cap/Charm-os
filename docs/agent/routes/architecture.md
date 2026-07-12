# Architecture Route

## 适用场景

- 架构讨论
- 能力归属与 Core 准入判断
- 分层、装配和边界争议
- 真实板级落地暴露的问题审计

## 最短阅读顺序

1. [`../../../CONSTITUTION.md`](../../../CONSTITUTION.md)
2. [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)
3. [`../../architecture/charm_core_semantic_audit.md`](../../architecture/charm_core_semantic_audit.md)
4. [`../../../README.md`](../../../README.md)
5. [`../../architecture/README.md`](../../architecture/README.md)
6. 与当前问题直接相关的 supporting 专题契约
7. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)
8. [`../rules/embedded-modern-cpp.md`](../rules/embedded-modern-cpp.md)
9. [`../glossary.md`](../glossary.md)
10. [`../skills/architect-review/SKILL.md`](../skills/architect-review/SKILL.md)
11. [`../workflows/architect-review-workflow.md`](../workflows/architect-review-workflow.md)

涉及同域通知或状态时，再读：

- [`../../architecture/signal_state_contract_v0.md`](../../architecture/signal_state_contract_v0.md)

问题来自真实板级 bring-up 时，再读：

- [`../../architecture/real_board_landing_gap_audit_v0.md`](../../architecture/real_board_landing_gap_audit_v0.md)
- [`../../architecture/capability_recovery_rules.md`](../../architecture/capability_recovery_rules.md)
- [`../../architecture/capability_recovery_matrix.md`](../../architecture/capability_recovery_matrix.md)

## 先不要做什么

- 不要绕过 Constitution 发明新 Core 名词。
- 不要从现有类、目录、Graph 或工具反推核心语义。
- 不要把 exploration 文档当作已获准契约。
- 不要只谈抽象理念，不核对仓库与消费者证据。
- 不要把局部板级 workaround 提升为跨平台契约。

## 完成前自检

- 给出六问答案和唯一裁决等级。
- 说明真实消费方以及为什么更小语义不够。
- 说明为什么不属于 Implementation / Tool 或 Project Fact。
- 明确证据、反例、例外预算和概念依赖。
- 指明结论应落入 canonical、supporting、exploration 还是 archive。
