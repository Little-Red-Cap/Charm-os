# `usb_storage_bundle` 设计草案

本文档定义 Charm 中首个建议落地的 bundle：`usb_storage_bundle`。

它的目标不是重新发明 USB MSC，而是把当前 Player / USB MSC 相关的能力组合，整理成一个正式、可复用、可观测、可装配的工程单元。

## 1. 为什么先做它

当前仓库里，USB Storage 场景已经暴露出典型问题：

- block device 绑定是一个单独动作
- USB 设备初始化是一个单独动作
- DCD adapter / board glue 是一个单独动作
- descriptors / strings / MSC config 也是单独拼的
- read_only / endpoint / ready hook 等配置散落在多个地方

结果是：

- 场景稍一复杂，就要反复拼接这些零件
- 同一类代码在 `profile`、`runtime`、`board`、`legacy glue` 之间来回分布
- 代码不是不能复用，而是复用边界不稳定

所以 `usb_storage_bundle` 应该成为：

> **围绕“把一个 block device 以 USB MSC 形式导出”这件事的正式工程抽象。**

## 2. Bundle 要覆盖的能力范围

`usb_storage_bundle` 应覆盖以下组合：

- block device 作为后端介质
- USB MSC 设备描述与 class config
- DCD / device adapter 绑定
- strings / vendor / product / serial 等设备信息
- 只读 / 可写策略
- ready callback / connect 行为
- 与场景相关的默认可观测性挂点

它不应覆盖：

- 文件系统挂载逻辑本身
- UI 逻辑
- 业务状态机
- 非 USB Storage 的媒体能力

## 3. 它在当前仓库中的映射对象

当前已经存在的散装能力包括：

- `Modules/io/usb/common/usb.msc_storage_bridge.cppm`
- `Modules/io/usb/class/usb.msc_block.node.cppm`
- `Examples/project/player/runtime/hqzy_cm7/usb_storage_bridge.cppm`
- `Examples/project/player/runtime/hqzy_cm7/usb_glue.cppm`
- `Examples/project/player/bsp/board_usb.cppm`
- `Examples/project/player/profiles/hqzy_cm7_usb_self_msc.system.cppm`

这些说明：

- 核心能力实际上已经存在
- 缺的是一个正式的组合层，而不是缺功能本身

## 4. 目标使用方式

最终理想使用方式不是：

- 手工写 `set_block_device`
- 手工配 strings
- 手工配 endpoints
- 手工接 on_ready
- 手工决定 graph 里加哪些 node

而是类似：

```text
profile usb_storage
    -> select runtime
    -> configure usb_storage_bundle
    -> bundle emits node set
    -> graph start
```

也就是：

- profile 只声明“我要一个 usb storage 场景”
- bundle 负责装配细节

## 5. 建议的 Bundle 模型

### 5.1 Bundle Config

`usb_storage_bundle` 至少应具备如下配置面：

- `cap_name`
- `block_cap`
- `read_only`
- `vendor_id`
- `product_id`
- `i_manufacturer`
- `i_product`
- `i_serial`
- `strings`
- `ep_out`
- `ep_in`
- `ep_mps`
- `connect_on_ready`
- `enable_observability`

这意味着：

- profile 可以控制场景差异
- bundle 可以控制默认组合结构
- runtime 只负责提供宿主事实，不再承担场景参数拼接

### 5.2 Runtime Requirements

`usb_storage_bundle` 应显式声明它依赖的 runtime 能力：

- `usb device controller`
- `usb device adapter`
- `block device handle`
- `system clock/time`（若需要轮询或状态驱动）

这一步非常重要，因为它能把“依赖什么”从经验知识变成显式 contract。

### 5.3 Produced Nodes

从 graph 视角看，它至少应产出：

- block storage 绑定相关 node（如需要）
- usb msc attach / start 相关 node
- 设备 ready / connect 行为绑定 node

换句话说：

bundle 对 graph 的主要贡献就是：

> **把 USB storage 相关 node 以稳定组合的方式产出**

## 6. 建议的内部拆分

为了避免 bundle 变成巨型黑盒，建议将它内部仍保持分层：

### 6.1 `descriptor layer`

负责：

- vendor/product/serial
- string table
- endpoint 参数

### 6.2 `binding layer`

负责：

- block device 绑定
- dcd / adapter / ready hook 绑定

### 6.3 `graph emission layer`

负责：

- 把上述配置转为具体 node / capability 输出

### 6.4 `observability layer`

负责：

- counter
- error snapshot
- setup / data stage trace hook

这样可以保证：

- bundle 是正式组合单元
- 但依然保持可拆、可测、可读

## 7. 对 `runtime` 的要求

为了让 `usb_storage_bundle` 真正成立，runtime 至少要做到：

- 提供稳定的 DCD 绑定对象
- 提供稳定的 adapter 绑定对象
- 提供 block device 获取方式
- 提供可选的 connect / reset / resume / suspend 胶水

也就是说，runtime 不再负责“场景拼装”，而是负责：

> **把 USB storage 场景所需宿主事实准备好。**

## 8. 对 `profile` 的要求

Profile 不应直接操心：

- `usb::msc::bridge::set_block_device()` 何时调用
- `UsbMscBlockInitChain` 的具体字段怎么填
- ready hook 该不该接
- endpoint 参数怎么设

Profile 应只负责：

- 选择这个 bundle
- 给 bundle 配置少量场景参数
- 决定运行模式和调试目标

例如：

- `usb_self_msc`：更强调 observability
- `usb_storage`：更强调最小导出路径
- `firmware_update_disk`：更强调只读策略与文件暴露策略

## 9. 可观测性建议

`usb_storage_bundle` 应该天然挂载以下观测点：

- `setup_count`
- `reset_count`
- `connect_count`
- `disconnect_count`
- `read_blocks`
- `write_blocks`
- `last_scsi_opcode`
- `last_error`
- `last_capacity_query`

并建议输出统一结构的快照接口，例如：

- `dump_state()`
- `dump_stats()`

这会显著降低后续 USB MSC 调试成本。

## 10. 与当前 Player 代码的迁移关系

### 当前已具备的前置条件

- `profile / runtime` 结构已经初步建立
- `usb_storage_bridge` 已经从 legacy `app/usb_system.cppm` 中迁出
- `usb_self_msc` 已经具备单独 profile 主线

### 下一步可执行迁移

建议分 3 步：

#### 第一步：先做文档级 bundle 边界收敛

目标：

- 明确哪些字段属于 bundle config
- 明确哪些对象属于 runtime requirement

#### 第二步：把 `usb_self_msc.system` 中的 USB MSC 拼装提炼成 bundle builder

目标：

- 不急着做大而全抽象
- 先从 Player 的真实使用路径里提炼 builder

#### 第三步：让 `usb_storage` / `usb_self_msc` 共用同一 bundle

目标：

- 让两个场景的差异只体现在 profile config 上
- 而不是体现在“各写一套装配方式”上

## 11. 当前阶段不要做的事

在首个 bundle 落地前，避免：

- 试图把 USB Audio 也一并塞进同一个 bundle
- 让 bundle 直接拥有 board 句柄生命周期
- 在 bundle 中掺入文件系统挂载逻辑
- 把所有调试打印直接硬编码进 bundle 主逻辑

首个 bundle 最重要的是：

- 边界清晰
- 能真实替换现有手工拼接路径
- 能服务至少两个相近场景

## 12. 一句话总结

`usb_storage_bundle` 的目标不是“把 USB MSC 代码集中到一起”，而是：

> **把“将 block device 以 USB MSC 形式导出”这件事，变成一个稳定、可声明、可观测、可复用的工程单元。**

如果这个 bundle 做成，Charm 才算真正跨过了从“有能力”到“复杂场景下顺手使用能力”的第一道坎。
