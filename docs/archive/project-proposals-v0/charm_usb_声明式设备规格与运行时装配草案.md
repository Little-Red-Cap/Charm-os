# Charm USB 声明式设备规格与运行时装配草案

## 0. 相关文档

这份草案与以下文档互补：

- [`charm_工程对象模型草案.md`](charm_工程对象模型草案.md)
- [`charm_foundation_runtime_与统一应用入口模型草案.md`](charm_foundation_runtime_与统一应用入口模型草案.md)
- [`usb_storage_bundle_设计草案.md`](usb_storage_bundle_设计草案.md)

## 1. 这份文档回答什么问题

这份文档用于回答一个已经被 `USB_SELF_MSC`、`USB_STORAGE`、`USB_AUDIO` 反复证明的问题：

> Charm 里的 USB 设备，最终应该如何被“更好用地声明、装配和启动”？

当前仓库里的 USB 代码已经证明了三件事：

- 纯手写 `main-usb-xxx.cpp` 能把问题打通，但维护成本高
- 纯底层 capability / graph 装配虽然正确，但工程使用门槛偏高
- 真正复杂的 bug 往往不是“少一行初始化”，而是缺少稳定的中间语义层

因此，理想目标不应只是“再做一套生成器”，而应是：

> **声明式规格 + 运行时装配 + 可覆写专家钩子**


## 2. 当前痛点

结合这轮 `USB MSC` 与 `USB Audio` 的真实推进，当前痛点已经很明确：

- 设备参数散落在 `main`、`board glue`、`class driver`、`runtime` 多处
- 描述符、端点、类实例、ready hook、storage binding 经常被手工拼接
- 同一种 USB 设备形态缺少统一表达，容易出现重复入口和重复实现
- 观测逻辑很值，但目前更多是“现场补钉”，而不是系统默认能力
- 调试成功后的真实语义，没有及时沉淀成稳定抽象

这会导致两种坏结果：

- 新场景开发速度慢
- 旧场景虽然能工作，但结构越来越难收敛


## 3. 目标形态

理想使用形态应该不是：

- 手工创建 `MscBot`
- 手工拼接 strings / descriptors
- 手工决定 endpoints
- 手工把 block device、DCD、glue、trace 接起来

而应该更接近：

```cpp
constexpr auto dev = usb::spec::device({
    .vid = 0x1209,
    .pid = 0x0002,
    .manufacturer = u"Charm",
    .product = u"Self MSC",
    .serial = u"0001",
    .functions = {
        usb::spec::msc({
            .block_cap = "block.sd0",
            .read_only = false,
            .removable = true,
            .io_window_size = 32768,
            .ep_mps = 64,
        }),
    },
});

auto runtime = usb::runtime::stm32_fs({
    .pcd = &hpcd_USB_OTG_FS,
    .dcd = player::stm32h7::board::usb_dcd_ops(),
    .adapter = &player::stm32h7::board::usb_adapter(),
});

auto app = usb::runtime::assemble(dev, runtime);
app.start();
```

也就是：

- 用户声明“我想要什么 USB 设备”
- 框架决定“要装配哪些具体对象”
- 用户只在少数需要时才介入覆写


## 4. 为什么不是先做一个重型 generator

“填参数直接生成”这个方向是对的，但不一定要先做外部代码生成器。

对于当前仓库，更现实也更稳的路线是：

- **规格声明**：描述设备是什么
- **运行时装配**：决定如何把真实对象接起来
- **描述符合成**：由装配层自动产出
- **专家覆写**：仅在少量高级场景下显式介入

也就是说，优先级更像：

1. 先把模型收对
2. 再决定是否需要额外 generator

如果模型对了，将来即使要做 generator，也只是输出同一套 `usb::spec`，而不是重新定义另一套世界观。


## 5. 建议的对象模型

### 5.1 `UsbDeviceSpec`

表示一个 USB 设备的上位规格，至少包括：

- `vid`
- `pid`
- `manufacturer`
- `product`
- `serial`
- `functions`
- 设备级策略，如默认 trace、字符串策略、复合设备策略

这个对象回答的是：

> 设备对外呈现成什么样子？


### 5.2 `UsbFunctionSpec`

表示一个 USB function 的声明式规格。

建议先支持：

- `msc`
- `audio`
- `cdc`

其中 `msc` 至少应支持：

- `block_cap`
- `read_only`
- `removable`
- `ep_mps`
- `io_window_size`
- `inquiry strings`

其中 `audio` 至少应支持：

- `sample_rate`
- `channels`
- `bits_per_sample`
- `feedback policy`
- `dma/ring policy`

这个对象回答的是：

> 设备里有哪些功能，每个功能怎么被参数化？


### 5.3 `UsbRuntimeBinding`

表示 USB 设备运行时真正落到哪条硬件/板级宿主链上。

对 STM32 FS 设备侧，它至少应包括：

- `PCD handle`
- `DCD ops`
- `device adapter`
- connect/disconnect/start/endpoint send/receive 所需 glue

这个对象回答的是：

> 这些声明式规格，最终如何落到具体板子与控制器上？


### 5.4 `UsbApp`

这是最终可启动对象。

它不再暴露大量底层拼接细节，而只暴露：

- `start()`
- `stop()`
- `diag()`
- `trace()`

也就是说，应用层真正使用的应该是：

> 一个已经装配完成的 USB 应用对象


## 6. 三层使用体验

### 6.1 快速档

适用于单功能设备。

例如：

- `usb::spec::msc(...)`
- `usb::spec::audio(...)`

目标是：

- 只填参数
- 不手写描述符
- 不手写 endpoint/open glue


### 6.2 组合档

适用于复合设备。

例如：

- `audio + msc`
- `cdc + msc`

目标是：

- 用户只列 functions
- 描述符与 interface 结构自动装配
- 复合设备身份、字符串与 endpoint 策略统一生成


### 6.3 专家档

适用于协议实验和深度调试。

应允许覆写：

- 自定义 descriptor provider
- 自定义 endpoint policy
- 自定义 class hook
- 自定义 trace / observability sink
- 特定 function 的 buffer / scheduling policy

目标是：

- 默认路径足够简单
- 深度用户仍可下探，不会被框架锁死


## 7. 当前代码到目标模型的映射

这一步非常关键，因为它决定这不是空中楼阁，而是从现有代码可演进得到的形态。

### 7.1 `board_usb.cppm`

当前角色：

- 板级 DCD glue
- PCD callback hook
- endpoint send/receive
- MSC pump 参与者

目标角色：

- **只保留板级 DCD/IRQ glue**
- 不再承载过多 class-specific 语义


### 7.2 `usb.device_driver.cppm`

当前角色：

- device 层标准请求与 descriptor provider
- endpoint callback schema
- class attach/open helper

目标角色：

- **成为正式的 USB device core 装配层**
- 承接 `UsbDeviceSpec -> device runtime object` 的转译


### 7.3 `usb.msc.cppm`

当前角色：

- BOT/SCSI 状态机
- MSC 读写窗口与 BOT 推进逻辑

目标角色：

- **保持为 MSC class driver 核心**
- 不再让场景入口直接理解太多其内部细节


### 7.4 `main-usb-self-msc.cpp`

当前角色：

- 场景装配
- 参数填写
- 一部分性能策略
- 一部分 observability

目标角色：

- **只保留场景级选择与少量观测**
- 渐进退化成：
  - 填规格
  - 绑定 runtime
  - 启动 app


## 8. 这轮真实调试已经沉淀出的“必须成为正式语义”的东西

这轮 `USB_SELF_MSC` 调试最值的地方，不只是把功能打通，而是明确了哪些语义必须升格成正式能力：

- `SET_ADDRESS` 生效时机不是细节，是 device 层正式 contract
- `Bulk IN busy` 不是临时补丁，是 BOT 数据阶段正式语义
- `IN complete -> continue pump` 不是现场技巧，是 MSC 传输推进 contract
- `data-in` 结束后何时进入 `CSW`，不是局部实现细节，而是 class 层核心规则
- `READ10` 的窗口大小不是单纯优化项，而是 MSC 可用性与性能模型的一部分
- `trace/diag` 不应继续只是入口里零散日志，而应成为 USB runtime 的可选标准能力

这些都说明：

> 当前阶段已经足够支撑一轮小而硬的收敛


## 9. 建议的第一轮落地范围

第一轮不追求把 USB 全部抽象完，只做一个非常克制的落地：

### 9.1 先把 `USB_SELF_MSC` 收成样板

目标形态：

- `usb::spec::msc(...)`
- `usb::runtime::stm32_fs(...)`
- `usb::runtime::assemble(...)`
- `app.start()`

也就是说：

- 先把最复杂、最有调试价值的 `MSC` 收成第一条样板链


### 9.2 暂不直接迁移自研 `USB Audio`

原因：

- `Audio` 比 `MSC` 更敏感，流控与反馈更复杂
- 先有稳定的 USB runtime/core，再转 `Audio` 成功率更高


### 9.3 保留专家入口

即使第一轮收敛，也应保留类似：

- 自定义 trace
- 自定义 buffer policy
- 自定义 class override

避免为了“简单使用”而牺牲协议实验能力。


## 10. 一个更接近最终形态的例子

### 10.1 单功能 MSC

```cpp
constexpr auto spec = usb::spec::device({
    .vid = 0x1209,
    .pid = 0x0002,
    .manufacturer = u"Charm",
    .product = u"Self MSC",
    .serial = u"0001",
    .functions = {
        usb::spec::msc({
            .block_cap = "block.sd0",
            .read_only = false,
            .removable = true,
            .io_window_size = 32768,
        }),
    },
});

auto runtime = usb::runtime::stm32_fs({
    .pcd = &hpcd_USB_OTG_FS,
    .dcd = player::stm32h7::board::usb_dcd_ops(),
    .adapter = &player::stm32h7::board::usb_adapter(),
});

auto app = usb::runtime::assemble(spec, runtime);
app.start();
```

### 10.2 Audio + MSC 复合设备

```cpp
constexpr auto spec = usb::spec::device({
    .vid = 0x1209,
    .pid = 0x0003,
    .manufacturer = u"Charm",
    .product = u"Audio + Storage",
    .serial = u"0002",
    .functions = {
        usb::spec::audio({
            .sample_rate = 48000,
            .channels = 2,
            .bits_per_sample = 16,
        }),
        usb::spec::msc({
            .block_cap = "block.sd0",
            .read_only = false,
            .removable = true,
        }),
    },
});
```

这个使用方式的关键不是语法糖，而是：

- 设备身份表达清楚
- function 组合表达清楚
- 装配边界清楚


## 11. 非目标

这份草案当前不主张立即去做：

- 大规模目录搬家
- 全仓 USB 一次性统一改造
- 先做一个复杂的外部 generator
- 先把 ST 栈与自研栈完全揉成一层

这轮更现实的目标是：

- 先把自研 USB core 的真实语义收稳
- 再让使用形态显著变简单


## 12. 建议的下一步实现顺序

1. 先把 `USB_SELF_MSC` 收成第一条 `spec + runtime + assemble` 样板链
2. 把当前 `trace/diag` 从场景入口提成正式 runtime 观测能力
3. 收敛 `board_usb.cppm` 到纯板级 DCD glue
4. 收敛 `usb.device_driver.cppm` 到正式 USB device 装配核心
5. 再评估是否开始把自研 `USB Audio` 纳入同一模型


## 13. 一句话总结

Charm USB 更好的最终使用形态，不应是继续手写各种 `main-usb-xxx.cpp`，也不应是一开始就押注重型代码生成器，而应是：

> **用声明式 `USB 设备规格` 表达“我要什么”，用运行时装配表达“它如何落地”，并保留少量专家覆写能力。**

这条路既能把当前已经验证的真实语义沉淀下来，也能让后续 `MSC / Audio / Composite` 的开发体验明显变好。
