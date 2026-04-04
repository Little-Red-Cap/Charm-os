# Charm 文档入门

本页是 Charm 的 10 分钟入门入口。  
读完本页后，再进入 `docs/architecture_overview.md` 或具体专题文档。

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
6. `docs/system/posix_support_overview.md`：如果你关心 Linux 用户态兼容，从这里进入

## 核心术语（最少认知）
- 能力（Capability）：可被依赖/装配的系统能力（如 `io.uart1`、`block.sd0`）
- InitGraph：装配器，按依赖顺序启动能力
- Channel/Reactor/Registry：IO 核心三件套

## 新增外设能力的标准路径
1) 在 `platform/board` 里补板级描述（UART/I2C/SPI/Flash）
2) 在 driver 层做适配（暴露为 Channel 或 block.device）
3) 在 init.graph 注册 `provides/requires`
4) 在上层通过 registry 打开能力（禁止直接拿全局）

## 接入文件系统的最短路径
1) 先接 `block.device`（SDMMC/SPI Flash/NOR/NAND）
2) 再接 `fs.vfs`（从 block.registry 拉设备）
3) 需要缓存就接 `block.cache`

## 接入 USB 的最短路径
1) 读 `docs/usb/usb_arch_plan.md`
2) 看 `docs/usb/usb_dsl_overview.md`
3) CDC/MSC 通过 device_driver 统一入口

## 常见坑（避免踩）
- 协议层禁止 busy-spin/自带超时
- Channel 禁止 Ok(0)
- 未注册能力禁止直接 init
- 依赖方向反转（protocol/io/at 禁止依赖 platform/hal）

## 继续深入
- IO 分层：`docs/io/io_layering_overview.md`
- 存储：`docs/storage/*`
- 系统：`docs/system/*`
- Linux 用户态兼容：`docs/system/posix_support_overview.md`
- 音频：`docs/system/charm_audio_architecture.md`
- 协作入口：`docs/agent/README.md`
- 代码审查：`docs/agent/skills/code-review/`
- 架构讨论：`docs/agent/skills/architect-review/`

## 下一步读什么
- 要开始改代码：先读 `docs/project/standards/项目C++编码要求.md`
- 要和 AI 协作：先读 `docs/agent/README.md`

## 暂时不用先读

刚进入项目时，不需要先深入以下文档：

- `docs/architecture/*` 的细则规则
- `docs/io/*` 的单点契约
- `docs/project/*` 的协作规范细节

先建立整体结构认知，再按任务进入具体专题。
