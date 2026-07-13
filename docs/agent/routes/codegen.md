# Codegen Route

## 文档状态

- `status`: `supporting`
- `scope`: 代码生成、模块骨架与接口扩展路由

## 最短路径

1. [协作规则](../rules/collaboration.md)
2. [嵌入式 C++ 规则](../rules/embedded-modern-cpp.md)
3. [架构规则](../rules/charm-architecture.md)
4. [Codegen skill](../skills/codegen/SKILL.md)
5. [输出模板](../templates/codegen-output.md)

涉及 signal/state 时补读 [对应契约](../../architecture/signal_state_contract_v0.md)。先明确 ownership、
依赖、错误与验证边界，再生成代码。
