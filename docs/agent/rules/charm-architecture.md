# Charm Architecture Rules

> **文档状态：`supporting`**

本文件只给 Agent 可执行的架构审查规则。最高裁决来自根
[`CONSTITUTION.md`](../../../CONSTITUTION.md) 与
[`charm_core_contract.md`](../../architecture/charm_core_contract.md)；专题行为由对应 contract 和源码负责。

## 事实与权威

- 先读 source、CMake、真实 target 和当次测试，再使用 README 或设计文档；
- 不从目录名、类型名、Graph、schema、report 或测试数量推断能力成立；
- supporting/exploration 文档不能覆盖 Constitution 或 canonical core contract；
- 更深目录的 `AGENTS.md` 可以补充局部操作规则，但不能改写 Core 身份。

## Core 准入

新增或提升公共概念前必须回答 Constitution 六问：跨运行环境稳定、真实消费者需要、可独立证明、
平台无关、例外预算低、概念依赖浅。没有通过时，保持为 Implementation / Tool、Project Fact、
exploration 或局部 contract。

特别禁止：

- 用现有实现反向证明它应进入 Core；
- 为 Backend、Driver、Provider、Graph、Compiler、Loader 建立全局宇宙模型；
- 把 H747、Host、QEMU 或某个产品的 profile 事实推广成跨平台语义；
- 因多个模块使用相似接口就先发明公共基类、Manager 或 Registry。

## 实现边界

- 依赖方向以真实 module imports 和 target wiring 判断，不使用抽象分层口号代替检查；
- 实时、ISR、task、reactor、host thread 是不同执行上下文，跨上下文必须使用明确 ingress；
- 内存、容器、atomic、锁和动态分配规则由具体执行路径约束，不做全仓一刀切；
- 错误必须保留并传播；具体 `Errc`、Result 或状态模型由专题 contract 定义；
- Project/BSP 拥有 startup、linker、vendor SDK、板级资源和产品 binding，Core 不吸收这些事实。

## 专题路由

| 问题 | 先读 |
|---|---|
| Capability 与 Core 归属 | [`../routes/capability.md`](../routes/capability.md) |
| init.graph 与装配 | [`../routes/init-graph.md`](../routes/init-graph.md) |
| Channel/Reactor/Registry | [`../routes/io.md`](../routes/io.md) |
| block device / storage | [`../routes/block-device.md`](../routes/block-device.md) |
| CMake、preset、target | [`../routes/build.md`](../routes/build.md) |
| signal/state/post | [`signal_state_contract_v0.md`](../../architecture/signal_state_contract_v0.md) |
| 语言与嵌入式 C++ 取舍 | [`embedded-modern-cpp.md`](embedded-modern-cpp.md) |

专题 route/contract 的规则只在其行为边界内成立。不要把某个 Channel、filesystem、clock、container
或 board 的约束复制成全仓默认。

## 审查检查点

- 改动的真实消费者和所有者是谁；
- 是否新增未经准入的 Core 名词或全局对象；
- 是否把 Project Fact、Backend 或工具细节泄漏到应用 contract；
- 是否跨执行上下文 direct call，或隐藏了生命周期/ownership；
- 失败语义是否稳定、可观察且未被吞掉；
- source/CMake/test 是否与文档声明一致；
- 验证是否覆盖实际环境，还是只检查了 metadata/build/schema；
- 是否可以用更小的局部实现解决，而不增加全仓词汇和例外。

旧的 16 节全仓规则归档于
[`../../archive/agent-guidance-v0/charm_architecture_rules_legacy.md`](../../archive/agent-guidance-v0/charm_architecture_rules_legacy.md)。
