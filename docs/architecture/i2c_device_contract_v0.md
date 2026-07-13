# I2C Device Interface v0

## 文档状态

- `status`: `supporting`
- `scope`: `io.device_i2c*` implementation interface
- `authority`: [`interface_admission_policy.md`](interface_admission_policy.md)

该接口是 driver/backend 试验边界，不是 Charm Core、Stable Boundary 或长期 ABI。旧 evidence
编排见 [`i2c-device-evidence-v0`](../archive/i2c-device-evidence-v0/README.md)。

## 模块

| module | 职责 |
|---|---|
| `io.device_i2c` | 操作接口、bus/device ref 与错误映射 |
| `io.device_i2c_mock` | 固定容量 transaction script backend |
| `io.device_i2c_hal` | `hal::I2cIoHandle` adapter |
| `io.device_i2c_facts` | 独立 facts 报告原型 |
| `driver.i2c_register_device` | 8-bit register consumer |
| `driver.i2c_whoami_probe` | identity probe consumer |

## API 与所有权

```cpp
write(address, tx) -> util::Result<void>
read(address, rx) -> util::Result<void>
write_read(address, tx, rx) -> util::Result<void>
```

`I2cBusRef` 保存 `void* + I2cBusOps*`，`I2cDeviceRef` 再绑定 `I2cAddress`；二者不拥有 backend。
`valid()` 只检查 ops 完整，不验证 address、controller、pinmux、clock 或目标存在。

- backend 执行 transaction 并映射平台错误；
- device ref 不管理 lock、power、recovery 或 backend 生命周期；
- consumer 不依赖 HAL handle、BoardCaps、mock 状态或 discovery descriptor；
- 调用为同步 `noexcept`，但不承诺 ISR-safe、reentrant、non-blocking 或 deadline；
- API 没有 timeout、cancel、async queue、arbitration policy 或 ownership token。

backend 自带的阻塞或 timeout 上限无法通过当前 interface 表达。

## 错误

`I2cErrorKind` 包含 bus、arbitration、address/data NACK、overrun、timeout、detach、policy、
unsupported 和 unknown，但公开结果只携带 `util::Errc`；多数 I2C-specific 错误折叠为 `io`。

| HAL status | `util::Errc` |
|---|---|
| `ok` | `ok` |
| `busy` | `busy` |
| `timeout` | `timeout` |
| `unsupported` | `not_supported` |
| 其它 | `io` |

调用方不能从 `I2cResult` 恢复原始 `I2cErrorKind`。

## Backend 与 consumer

`I2cScriptBus<MaxOps, MaxTx, MaxRx>` 按顺序验证操作、address、TX 和 RX size，并可注入错误。
容量超限返回 `buffer_overflow`；调用超出脚本或参数不符会记录首个 script error。它不模拟电气、
clock stretching、arbitration 或 controller 时序。

`HalI2cBus` 直接转发 `hal::i2c_write/read/write_read`，只证明 HAL handle 可适配到相同调用面。
`RegisterDevice8` 和 `WhoAmIProbe` 证明 consumer 可只依赖 `I2cDeviceRef`；identity mismatch 返回
`bad_state`。这些不是具体芯片 driver 或真实板证据。

## Facts 边界

`io.device_i2c_facts` 记录 bus/controller/device/clock/pinmux/power/backend/evidence facts，并统计
required/provided/missing。它不创建 `I2cBusRef`，不解析 provider identity、冲突或 binding；facts
也可能与板上状态不同。因此它是报告 sidecar，不是 interface 前置条件。

## 证据与缺口

| smoke | 覆盖 |
|---|---|
| `i2c_contract_mock_smoke` | ref、transaction 及失败路径 |
| `i2c_hal_adapter_smoke` | HAL status 与调用投影 |
| `i2c_register_driver_smoke` | register consumer |
| `i2c_whoami_probe_smoke` | identity、IO 错误和 mismatch |
| `i2c_facts_smoke` | facts resolution 与 sidecar 形状 |

尚未证明真实 controller/pinmux/IRQ/DMA、bus sharing/recovery、7/10-bit address policy、timeout/cancel、
并发/ISR 安全、具体芯片 driver 或跨平台 ABI。
