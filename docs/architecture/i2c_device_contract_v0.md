# I2C 局部实现边界 v0

## 文档状态

- `status`: `supporting`
- `scope`: `io.device_i2c*` implementation interface
- `authority`: [`interface_admission_policy.md`](interface_admission_policy.md)

该接口是 driver/backend 试验边界，不是 Charm Core、Stable Boundary 或长期 ABI。旧 evidence
编排见 [`i2c-device-evidence-v0`](../archive/i2c-device-evidence-v0/README.md)。

## 当前形状

[`io.device_i2c`](../../Modules/io/device/io.device_i2c.cppm) 提供同步的 `write/read/write_read`，
bus/device ref 都是 non-owning。`valid()` 只检查 ops 完整，不验证 address、controller、pinmux、clock
或目标存在。

- backend 执行 transaction 并映射平台错误；
- device ref 不管理 lock、power、recovery 或 backend 生命周期；
- consumer 不依赖 HAL handle、BoardCaps、mock 状态或 discovery descriptor；
- 调用为同步 `noexcept`，但不承诺 ISR-safe、reentrant、non-blocking 或 deadline；
- API 没有 timeout、cancel、async queue、arbitration policy 或 ownership token。

backend 内部的阻塞和 timeout 上限无法通过当前接口表达。

## 错误边界

公开结果只携带 `util::Errc`。timeout、detach、policy 和 unsupported 分别映射为通用错误；bus、
arbitration、NACK、overrun 和 unknown 折叠为 `io`。HAL adapter 还保留 busy。调用方不能恢复原始
`I2cErrorKind`。

## 已有证据

`I2cScriptBus<MaxOps, MaxTx, MaxRx>` 按顺序验证操作、address、TX 和 RX size，并可注入错误。
容量超限返回 `buffer_overflow`；调用超出脚本或参数不符会记录首个 script error。它不模拟电气、
clock stretching、arbitration 或 controller 时序。

`HalI2cBus` 直接转发 `hal::i2c_write/read/write_read`，只证明 HAL handle 可适配到相同调用面。
`RegisterDevice8` 和 `WhoAmIProbe` 证明 consumer 可只依赖 `I2cDeviceRef`；identity mismatch 返回
`bad_state`。这些不是具体芯片 driver 或真实板证据。

`io.device_i2c_facts` 记录 bus/controller/device/clock/pinmux/power/backend/evidence facts，并统计
required/provided/missing。它不创建 `I2cBusRef`，不解析 provider identity、冲突或 binding；facts
也可能与板上状态不同。因此它是报告 sidecar，不是 interface 前置条件。

验证入口位于 `Examples/io/i2c_*_smoke`，分别覆盖 scripted transaction、HAL adapter、register consumer、
identity probe 和 facts sidecar。

## 未证明

尚未证明真实 controller/pinmux/IRQ/DMA、bus sharing/recovery、7/10-bit address policy、timeout/cancel、
并发/ISR 安全、具体芯片 driver 或跨平台 ABI。
