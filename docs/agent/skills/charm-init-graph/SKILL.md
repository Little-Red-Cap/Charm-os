---
name: charm-init-graph
description: 新增或更新 Charm 的 init.graph 节点、board_caps 与 bringup 链路。用于新增 UART/SPI/I2C/Flash/USB 等能力并接入 init.graph。
---

# Charm InitGraph 工作流

## 适用范围
当你需要新增板级能力，并通过 init.graph、board_caps、bringup 链路把能力装配进系统时，使用本技能。适用于 UART/SPI/I2C/Flash/USB 等板级能力的标准接入路径。

## 不可妥协项
- 禁止隐式全局（使用 Context/registry）。
- 禁止在 init.graph 之外直接 init。
- provides/requires 必须显式且一致。
- Capability 名称是稳定字符串，不得随意变体。

## 步骤（最小顺序）
1) **定义 board caps**
   - 增补或扩展 `platform/board/<board>` 的 caps 结构。
   - 为能力提供描述符（UART/I2C/SPI/Flash）。

2) **驱动适配层**
   - 在 driver 层实现适配（Channel 或 block.device）。
   - 保证 non-blocking 语义与显式错误处理。

3) **init.node 注册**
   - 创建包含 `provides`/`requires` 的节点。
   - 保持 phase 顺序：platform -> hal -> driver -> service/app。

4) **Bringup 链**
   - 按目标的 `init.graph` 和 bringup wiring 接入节点。
   - Bringup 只负责链路组合，避免在 main 写自定义 init。

5) **Registry 接线**
   - 在 driver node 的 init 中注册 endpoint 到 `io.registry` 或 `block.registry`。
   - 消费端按名称打开（禁止从 platform 直拿句柄）。

## Capability 命名规则
- 使用稳定命名：`platform.irq`、`hal.uart1`、`io.uart1`、`block.sd0`。
- 避免别名，确需别名时用 replace_channel 或 registry 映射。

## 验证清单
- Win stub 路径构建通过。
- `registry.open(...)` 能找到新增能力。
- 协议层不直接引用 platform/hal。

## 常见陷阱
- IRQ 提前启用，channel 尚未注册。
- Channel read/write 返回 Ok(0)。
- 把 core 节点塞进 extra node。

## 参考
- docs/overview.md
- docs/system/init_graph_contract.md
- docs/io/io_channel_contract.md
- docs/io/io_reactor_contract.md
- docs/io/io_registry_contract.md
