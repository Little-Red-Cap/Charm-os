# 早期全仓 Review 保留笔记

> `status`: `archived`

旧 review 的文件行数、路径、优先级和 backlog 已失效。本文只保留跨版本可复用的审查方法；当前结论
必须重新检查 source、CMake、真实 target 与当次测试。

## 聚合与拆分

入口宽不自动构成问题。先用真实 import 和 target wiring 区分稳定入口、迁移兼容、产品聚合、内部组装
和退役 tombstone，再判断是否拆分。

大文件按变化原因和失败边界拆，不按行数切片。常见边界包括 schema/storage/executor、
facade/builder/runtime payload、state machine/observation、protocol/backend bridge 和
implementation/C bridge/disabled stub。第一刀优先只搬家并保持公共名称与行为；接口变化和旧文件清理
分开提交。

## Observation、Bridge 与 Stub

- 核心状态机只拥有最小 trace 数据；JSON、CSV、report 和 sink 放在观察模块。
- Debug self-check 与 synthetic provider 进入内部验证模块、host smoke 或显式 debug target，不扩大生产 API。
- Core adapter 负责真实协议或库语义；bridge 只拥有外部 ABI、全局 callback 或 trampoline；stub 明确
  返回 feature unavailable，不伪装成功。
- Target 选择实现，避免在业务逻辑中扩散条件编译。

## Type Erasure

`void* context + ops table + trampoline` 适合 C ABI、vendor callback、跨语言或异构边界。普通 C++ 组合
优先 typed ref、concept 或局部 provider view。使用类型擦除时必须说明 context owner/lifetime、ops 完整性、
解绑和并发行为、错误传播，以及 typed boundary 不适用的原因。

## 分刀顺序

1. 核对当前消费者、target wiring 与失败证据。
2. 固定现有 public surface，先做无语义变化的内部拆分。
3. 为窄边界补 contract、smoke 和失败路径。
4. 独立提交接口迁移、兼容清理和命名调整。

任何新治理任务都必须从当前代码重新建立证据，不能复用旧 backlog 的完成结论。
