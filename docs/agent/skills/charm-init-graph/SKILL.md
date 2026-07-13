---
name: charm-init-graph
description: 审查或接入已经选择 init.graph 的静态初始化目标。
---

# Charm Init Graph

> status: `supporting`

只在目标已经使用 `init::Graph`，或源码证明需要静态依赖图时使用本 skill。动态 discovery、hot-plug、
runtime lookup 和普通应用对象不应为了统一形式强行进入 init graph。

## 接线前

1. 读取 target/profile CMake、startup path、现有 node 和真实 consumer。
2. 明确 owner、初始化 lifetime、执行阶段和失败后状态。
3. 读取 [init graph contract](../../../system/init_graph_contract.md) 与实际 `init.node/init.graph` source。
4. 判断依赖是否真的是启动期静态关系；不是则回到对应 runtime owner。

## 接线

- node 只声明实际提供和需要的最小 capability，由 owning target/profile 绑定；
- phase/runlevel 来自 target 行为，不从目录层级推导；
- missing 或 duplicate provider 必须显式失败，不能增加 fallback 隐藏装配错误；
- registry endpoint 只在 consumer 需要对应 registry contract 时发布；
- board/HAL handle 与 vendor state 留在 project/backend adapter。

CapId string 是图内 hashed identifier，不是全局 Charm capability namespace。名称由 owner 维护，并只测试
目标实际面临的重复、碰撞和容量风险。

## 验证

至少覆盖目标正向顺序、受影响的解析负例、首个 init failure 和 phase/runlevel filtering。具体错误集合、
容量与停止行为由 contract/source 决定，不在 skill 复制。

若结论包含平台启动，还必须运行对应 target；Host graph fixture 不证明 IRQ、clock、peripheral 或 board
readiness。

## 输出

记录 node owner、target、provides/requires、phase/runlevel、失败行为和验证域。configure 成功不能替代
初始化执行证据。
