# Charm 文档入门

这是一份给新同学的最短路径文档，目标是让你 10 分钟内知道：
- 这个项目解决什么问题
- 代码应该从哪里开始看
- 新功能应该怎么接入（不会破坏架构）

## 一句话理解
Charm 是一个面向 MCU/PC 的模块化系统框架，核心是“能力图 + 非阻塞 IO + 统一装配”。

## 读代码的最短路径
1. `docs/architecture_overview.md`：全局结构与分层
2. `docs/system/init_graph_contract.md`：系统装配规则（所有能力必须注册）
3. `docs/io/io_channel_contract.md`：非阻塞通道契约
4. `docs/io/io_reactor_contract.md`：事件驱动 IO
5. `docs/input/input_layering_decision.md`：输入链路统一

## 核心术语（最少认知）
- 能力（Capability）：可被依赖/装配的系统能力（如 io.uart1、block.sd0）
- InitGraph：装配器，按依赖顺序启动能力
- Channel/Reactor/Registry：IO 核心三件套

## 我想新增一个外设能力
1. 在 `platform/board` 里添加板级描述（例如 UART/I2C/SPI）
2. 在 driver 层做适配（把外设暴露为 Channel/BlockDevice）
3. 在 init.graph 里注册 provides/requires
4. 在上层通过 registry open 能力（禁止直接拿全局）

## 我想接入文件系统
1. 先接 `block.device`（SDMMC/SPI Flash/NOR/NAND）
2. 再接 `fs.vfs`（从 block.registry 拉设备）
3. 需要缓存就接 `block.cache`

## 我想接入 USB
1. 先读 `docs/usb/usb_arch_plan.md`
2. 再看 `docs/usb/usb_dsl_overview.md`
3. CDC/MSC 都走 device_driver 的统一入口

## 常见坑（避免踩）
- 协议层禁止 busy-spin/自带超时
- Channel 禁止 Ok(0)
- 未注册能力禁止直接 init

## 继续深入
- IO 分层：`docs/io/io_layering_overview.md`
- 存储：`docs/storage/*`
- 系统：`docs/system/*`
