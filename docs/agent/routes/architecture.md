# Architecture Route

## 适用场景

- 架构讨论
- 能力归属判断
- 分层、装配、边界争议
- 真实板级落地暴露的问题审计
- 能力回收顺序与默认路径失配排查

## 最短阅读顺序

1. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)
2. [`../rules/embedded-modern-cpp.md`](../rules/embedded-modern-cpp.md)
3. [`../../architecture/signal_state_contract_v0.md`](../../architecture/signal_state_contract_v0.md)（涉及同域通知 / 状态 / post 时）
4. [`../glossary.md`](../glossary.md)
5. [`../skills/architect-review/SKILL.md`](../skills/architect-review/SKILL.md)
6. [`../workflows/architect-review-workflow.md`](../workflows/architect-review-workflow.md)
7. [`../templates/architect-review-output.md`](../templates/architect-review-output.md)

如果当前问题来自真实板级 bring-up，而不是纯抽象讨论，再补：

8. [`../../architecture/real_board_landing_gap_audit_v0.md`](../../architecture/real_board_landing_gap_audit_v0.md)
9. [`../../architecture/capability_recovery_rules.md`](../../architecture/capability_recovery_rules.md)
10. [`../../architecture/capability_recovery_matrix.md`](../../architecture/capability_recovery_matrix.md)

## 先不要做什么

- 不要只谈抽象理念，不落到仓库现状。
- 不要把第三方框架对照直接当成设计答案。
- 不要跳过 tradeoff 和风险。
- 不要把 `EvidenceRig` 的局部 workaround 直接当成 Charm 通约契约。

## 完成前自检

- 说明推荐归属层。
- 说明为什么不放在其它层。
- 明确当前前提、风险和后续文档落点。
- 如果问题来自真实板级落地，明确它属于“能力缺失”“能力发现性”“接缝阻力”“默认路径失配”中的哪一类。
