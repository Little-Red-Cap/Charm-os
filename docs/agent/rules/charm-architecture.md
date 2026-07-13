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
- 在没有共同 consumer 和行为契约时，把 Backend、Driver、Provider、Graph、Compiler 或 Loader
  统一为全局抽象；
- 把 H747、Host、QEMU 或某个产品的 profile 事实推广成跨平台语义；
- 因多个模块使用相似接口就先发明公共基类、Manager 或 Registry。

## 实现边界

- 依赖方向以真实 module imports 和 target wiring 判断，不使用抽象分层口号代替检查；
- 实时、ISR、task、reactor、host thread 是不同执行上下文，跨上下文必须使用明确 ingress；
- 内存、容器、atomic、锁和动态分配规则由具体执行路径约束，不做全仓一刀切；
- 错误必须保留并传播；具体 `Errc`、Result 或状态模型由专题 contract 定义；
- Project/BSP 拥有 startup、linker、vendor SDK、板级资源和产品 binding，Core 不吸收这些事实。

专题阅读路径由 [architecture route](../routes/architecture.md) 选择，评审步骤由
[architect-review skill](../skills/architect-review/SKILL.md) 维护。专题 contract 的局部约束不能被复制成
全仓默认。
