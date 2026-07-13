# 声明式 USB 早期取舍保留笔记

> status: `archived`
>
> scope: 早期 USB spec/runtime proposal 中仍可复用的设计理由，不说明当前功能

当前实现边界见 [`usb_dsl_overview.md`](../../usb/usb_dsl_overview.md) 和
[`usb_architecture_overview.md`](../../usb/usb_architecture_overview.md)。字段、支持的 class、错误码和
验证范围以 source 与当次 smoke 为准。

## 先模型，后 Generator

早期提案拒绝先做重型外部 generator。原因是 descriptor、endpoint、class instance 和 runtime binding
若尚无稳定模型，generator 只会冻结另一套输入语言。合理顺序是：

1. spec 表达设备身份与 function intent；
2. model/plan 分配 interface、endpoint 并检查 target constraints；
3. runtime binding 注入 DCD、board glue、storage 与执行资源；
4. descriptor builder 生成 wire data；
5. 只有重复输入已稳定且有真实消费者时，再考虑 generator。

Generator 若出现，也应生成或消费同一模型，不能建立第二套 USB 世界。

## Spec 与 Runtime Binding

设备对外形状和平台落地事实需要分开：

- spec/model 拥有 VID/PID、strings、functions、endpoint intent 和 class policy；
- plan 拥有 target constraints、编号分配和冲突检查；
- runtime binding 拥有控制器、DCD/HCD、IRQ/DMA/cache、block/channel backend 与生命周期；
- app/profile 只选择组合，不手写协议状态机。

spec/plan 成功只能证明模型在当前约束下可构造，不能证明真实控制器枚举、主机兼容或 data path。

## 专家入口

默认路径应减少 descriptor 与 endpoint glue 的重复，但不能封死协议实验。局部 expert override 可用于：

- descriptor provider 或 target-specific constraints；
- endpoint/buffer/scheduling policy；
- trace 与 observability sink；
- 尚未进入通用模型的 class hook。

覆写必须停留在明确 adapter 或 function 边界，不能让默认 spec 依赖某块板、某个 class driver 或现场
调试变量。

## MSC Storage 组合边界

“将一个 block device 以 USB MSC 导出”需要组合 storage backend、read-only policy、inquiry identity、
USB descriptor/class config、controller binding 与 ready/connect 生命周期。它不应拥有：

- 文件系统 mount 或文件暴露策略；
- UI 与业务状态机；
- board handle 的生命周期；
- 与 MSC 无关的媒体能力。

block device 的 readiness、capacity、block size、read/write failure 和只读行为应由 storage/class bridge
明确传播。观测可以记录 setup/reset、SCSI command、block IO 和最后错误，但不能用日志成功替代 BOT
状态、backend 返回值或真实主机传输证据。

## 调试沉淀出的协议边界

早期 MSC bring-up 暴露了几类不应留在场景 `main` 中的语义：

- `SET_ADDRESS` 的实际生效必须服从 control transfer 时序，而不是收到 request 时任意写硬件；
- Bulk IN busy、completion 和下一次 pump 之间需要明确 ownership/backpressure；
- data-in 结束到 CSW ready/send 的转换属于 BOT 状态机，不是 board glue；
- READ(10) window 同时影响固定内存预算、吞吐与 short/error 行为；
- trace token 应描述协议状态和错误，不依赖临时 printf 文本。

这些判断不冻结当前函数名或实现步骤。现行行为必须从 device/class source、trace vocabulary、mock/replay
和真实板证据重新确认。

## 不应从旧提案推断

- UAC、CDC、MSC 或 composite 已达到同等级 runtime 支持；
- 任意 DCD/HCD 都满足默认 FS constraints；
- descriptor 生成成功等于设备可枚举；
- trace/replay 等于真实 IRQ、DMA、cache 或主机证据；
- 旧 `UsbApp`、`assemble()` 伪 API 是必须恢复的公共接口。
