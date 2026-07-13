# 声明式 USB 早期取舍保留笔记

> `status`: `archived`

当前实现见 [`usb_dsl_overview.md`](../../usb/usb_dsl_overview.md) 和
[`usb_architecture_overview.md`](../../usb/usb_architecture_overview.md)。本文只保留早期设计理由；字段、
class 支持、错误码和验证结果以 source 与当次 smoke 为准。

## Model Pipeline

```text
spec -> model/plan -> runtime binding -> descriptor builder
```

Spec 表达 device identity 与 function intent；model/plan 分配 interface/endpoint 并检查 target constraints；
runtime binding 注入 DCD/HCD、IRQ/DMA/cache、storage/channel backend 与 lifecycle；builder 生成 wire data。

在模型稳定且出现重复消费者前，不引入外部 generator。Generator 只能消费或生成同一模型，不能建立
第二套 USB 语义。Plan 成功只证明模型满足当前约束，不证明真实枚举、主机兼容或 data path。

## Ownership 与 Expert Override

- spec/model 拥有 VID/PID、strings、function intent 和 class policy；
- plan 拥有编号分配、target constraints 与冲突检查；
- runtime binding 拥有 controller、board glue、backend 与生命周期；
- app/profile 只选择组合，不手写协议状态机。

Descriptor provider、target constraints、endpoint/buffer scheduling、trace sink 或实验 class hook 可以作为
局部 expert override，但必须停留在明确 adapter 边界，不能让默认 spec 依赖具体板卡或现场调试变量。

## MSC 与 BOT 边界

MSC 组合 block device、read-only policy、inquiry identity、descriptor/class config、controller binding 和
ready/connect lifecycle；它不拥有 filesystem、UI、业务状态机或 board handle lifetime。

早期 bring-up 固定了以下边界：

- `SET_ADDRESS` 在 control transfer 规定时点生效；
- Bulk IN busy/completion/pump 有明确 ownership 与 backpressure；
- data-in 到 CSW 的转换属于 BOT state machine，不属于 board glue；
- READ(10) window 同时约束内存、吞吐和 short/error 行为；
- storage readiness、geometry、read/write failure 和只读状态必须穿过 class bridge。

Trace/replay 只能描述协议状态和错误，不能替代 BOT 返回值、真实 IRQ/DMA/cache 或主机传输证据。
旧提案也不能证明 UAC/CDC/MSC/composite 具有同等级 runtime 支持，或要求恢复旧 `UsbApp/assemble()` API。
