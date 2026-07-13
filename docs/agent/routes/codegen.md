# Codegen Route

## 适用场景

- 代码生成
- 模块骨架设计
- 现有接口的增量扩展

## 最短阅读顺序

1. [`../rules/collaboration.md`](../rules/collaboration.md)
2. [`../rules/embedded-modern-cpp.md`](../rules/embedded-modern-cpp.md)
3. [`../rules/charm-architecture.md`](../rules/charm-architecture.md)
4. [`../../architecture/signal_state_contract_v0.md`](../../architecture/signal_state_contract_v0.md)（涉及事件连接时）
5. [`../skills/codegen/SKILL.md`](../skills/codegen/SKILL.md)
6. [`../templates/codegen-output.md`](../templates/codegen-output.md)

## 先不要做什么

- 不要在架构不清时直接开写。
- 不要为了局部方便破坏分层、初始化纪律或 IO 纪律。
- 不要跳过文档同步。

## 完成前自检

- 能力归属是否清晰。
- 初始化、依赖方向、错误模型是否一致。
- 行为变化是否同步回写文档。
