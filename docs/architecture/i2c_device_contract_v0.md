# I2C Device Interface v0

> status: `supporting`
>
> 本文是当前 I2C implementation interface 的状态卡，不定义 Charm Core、Stable Boundary
> 或长期 ABI。

## 文档角色

本文描述当前 `io.device_i2c*` implementation interface。它是 driver/backend 试验边界，不是 Charm Core，也未获准成为 `Stable Boundary` 或长期 ABI。

接口审查规则见 [`interface_admission_policy.md`](interface_admission_policy.md)；旧 maturity/evidence 编排见 [`../archive/i2c-device-evidence-v0/README.md`](../archive/i2c-device-evidence-v0/README.md)。

## 源码边界

| 模块 | 当前职责 |
|---|---|
| `io.device_i2c` | 三操作接口、type-erased bus ref、address-bound device ref、错误映射 |
| `io.device_i2c_mock` | 固定容量 transaction script backend |
| `io.device_i2c_hal` | `hal::I2cIoHandle` 到 interface 的适配 |
| `io.device_i2c_facts` | 独立 I2C facts 报告原型 |
| `driver.i2c_register_device` | 8-bit register consumer |
| `driver.i2c_whoami_probe` | register identity probe consumer |

## API

`I2cBus` 和 `I2cBusOps` 当前固定三个同步调用：

```cpp
write(address, tx) -> util::Result<void>
read(address, rx) -> util::Result<void>
write_read(address, tx, rx) -> util::Result<void>
```

`I2cBusRef` 保存 `void* + I2cBusOps*`，`I2cDeviceRef` 再绑定一个 `I2cAddress`。二者均不拥有 backend；调用方必须保证 backend 生命周期覆盖 ref。

`I2cDeviceRef::valid()` 只检查 bus ops 完整，不验证 address、controller 状态、pinmux、clock 或目标设备存在。

## Ownership 与执行

- backend 负责一次调用的 transaction 执行与平台错误映射；
- `I2cDeviceRef` 只保存 address，不管理 bus lock、power 或 recovery；
- consumer 不应依赖 HAL handle、BoardCaps、mock 内部状态或 discovery `DeviceDesc`；
- 所有接口为 `noexcept`，结果通过 `util::Result<void>` 返回；
- 当前调用模型是同步返回，但源码不承诺 ISR-safe、reentrant、non-blocking 或 deadline；
- 没有公共 timeout 参数、cancel、async queue、bus arbitration policy 或 ownership token。

如果 backend 内部阻塞或自带 timeout，当前 interface 无法表达其上限。这是尚未解决的语义缺口，不应由文档假定。

## 错误

`I2cErrorKind` 当前包含：

- `bus`
- `arbitration_lost`
- `nack_address/nack_data`
- `overrun`
- `timeout`
- `target_detached`
- `policy_violation`
- `unsupported`
- `unknown`

但公开 `I2cResult` 只携带 `util::Errc`；多数 I2C-specific kind 会折叠为 `Errc::io`。调用方无法从结果恢复原始 `I2cErrorKind`。

HAL adapter 当前映射：

| HAL status | `util::Errc` |
|---|---|
| `ok` | `ok` |
| `busy` | `busy` |
| `timeout` | `timeout` |
| `unsupported` | `not_supported` |
| 其它 | `io` |

因此 `I2cErrorKind` 与 HAL status 并非一一对应 taxonomy。

## Backend 与 Consumer

### Script backend

`I2cScriptBus<MaxOps, MaxTx, MaxRx>` 预置期望 transaction，按顺序验证 kind、address、TX bytes 和 RX size，并可注入 `I2cErrorKind`。队列和 payload 超限返回 `buffer_overflow`；调用超出脚本、顺序错误或参数不符会记录首个 script error。

它验证调用语义，不模拟电气、时序、clock stretching、arbitration 或真实 controller 状态。

### HAL adapter

`HalI2cBus` 直接转发 `hal::i2c_write/read/write_read`。它证明现有 HAL handle 可以投影到相同调用面，不是第二个独立硬件实现，也不证明任何真实板工作。

### Consumers

- `RegisterDevice8<MaxPayload>` 实现单字节 register 读写和固定容量 burst；
- `WhoAmIProbe` 读取一个 register，并以 `bad_state` 表示 identity mismatch。

二者证明消费代码可以只依赖 `I2cDeviceRef`，但仍是通用小型 consumer，不代表具体芯片 driver。

## Facts 原型

`io.device_i2c_facts` 定义 bus/controller/device/clock/pinmux/power/backend/evidence 等 fact kind，并对 required/provided/missing 做计数。

当前限制：

- facts 不参与 `I2cBusRef` 创建或调用；
- `resolve_i2c_facts()` 只做列表统计，不解析 provider identity、冲突或 binding；
- fact names 没有经 Constitution 裁决；
- facts sidecar 与 real board 状态可能不一致。

因此它是报告实验，不是 I2C interface 的成立前提。

## Host 证据

| 示例 | 证明范围 |
|---|---|
| `Examples/io/i2c_contract_mock_smoke` | ref 与 transaction script 的成功/失败路径 |
| `Examples/io/i2c_hal_adapter_smoke` | HAL status 和调用投影 |
| `Examples/io/i2c_register_driver_smoke` | register consumer 不依赖 HAL/mock 类型 |
| `Examples/io/i2c_whoami_probe_smoke` | identity 成功、错误和 mismatch |
| `Examples/io/i2c_facts_smoke` | fact resolution 与 sidecar 输出形状 |

Host fixture、artifact report 和 compare 脚本只证明 evidence 工具链输入形状，详见归档。它们不是 real-board evidence。

## 未证明

- 真实 controller、pinmux、clock、power、IRQ 或 DMA；
- bus sharing、locking、multi-master 和 recovery；
- 7-bit/10-bit address policy；
- timeout 上限、cancel、async/reactor 模型；
- thread/ISR/reentrancy 安全；
- 具体 sensor、EEPROM、PMIC 或 codec driver；
- public ABI、`Stable Boundary` 或 Core 身份。

下一步若继续推进，应选择一个真实 consumer，先用 script backend 覆盖正反路径，再用真实板记录验证同一行为；是否申请稳定边界在证据完成后单独裁决。
