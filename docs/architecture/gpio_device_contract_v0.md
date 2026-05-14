# GPIO Device Contract v0

## 定位

本文记录 Charm 设备契约窄腰中的 GPIO proposed contract card。

它不是 admitted 公共 ABI，也不是当前 `hal_gpio` 的重命名。
它只回答一个边界问题：

> **GPIO 作为 driver-facing 公共契约时，哪些语义属于 pin，哪些语义应该上交给 input/service/runtime。**

当前代码中已经存在：

- `Modules/io/hal/hal_gpio.cppm`
- `Modules/io/hal/hal_input.cppm`
- `Modules/io/input/input.raw_sampler.cppm`
- `Modules/io/input/input.key.cppm`

但这些都不等价于本文描述的公共 GPIO device contract。

## 1. 当前等级

当前等级是 `proposed`。

它已经具备：

- controller-facing HAL：`hal_gpio`
- 输入分层文档明确了 `input.service` 与 UI 语义边界
- 窄腰总览中已经提出 `GpioInput / GpioOutput / GpioEdgeSource`

它还不是 `experimental`，因为仍然缺：

- driver-facing GPIO input/output/edge contract
- mock backend
- HAL adapter backend
- 至少一个只依赖 GPIO contract 的 driver / component
- no-hardware smoke
- contract-local facts vocabulary
- artifact / evidence 投影样例

## 2. Contract Shape

GPIO proposed contract 不应该把 pin 做成万能对象。

它应拆成三个语义面：

- `GpioInput`
- `GpioOutput`
- `GpioEdgeSource`

推荐理解：

```text
GpioInput       -> read current level
GpioOutput      -> set output level
GpioEdgeSource  -> publish/notify edge occurrence
```

这三者可以由同一颗物理 pin 支撑，但不应在公共 contract 中合并成一个无边界的 `GpioPin`。

## 3. Ownership And Responsibility

### 3.1 `GpioInput`

`GpioInput` 负责表达：

- 当前 pin 可以读取电平
- 输入方向、pull、pinmux 等底层事实已经由 backend 或 binding 管理
- 调用方能得到公共 level 语义，而不是平台寄存器位

`GpioInput` 不负责：

- 去抖
- 长按 / 短按
- 重复触发
- UI intent
- input routing
- 轮询调度

这些属于 `input.service`、采样层或 UI/domain 语义。

### 3.2 `GpioOutput`

`GpioOutput` 负责表达：

- 当前 pin 可以设置输出电平
- 输出方向、初始值、驱动能力等底层事实已经由 backend 或 binding 管理
- 调用返回时 output request 已被 backend 接受或失败

`GpioOutput` 不负责：

- PWM
- LED pattern
- blink timer
- display backlight policy
- power sequencing policy

这些应由更高层 service 或 driver 组合。

### 3.3 `GpioEdgeSource`

`GpioEdgeSource` 负责表达：

- 当前 pin 可以作为边沿事件来源
- 上升沿、下降沿或双边沿选择由 backend / binding / policy 管理
- ISR 或 backend 只投递 edge occurrence，不直接执行上层语义

`GpioEdgeSource` 不负责：

- 在 ISR 中执行完整 driver 逻辑
- 去抖
- click / long press
- focus navigation
- UI action
- 自建事件队列

edge source 应进入 reactor、EDA、input service 或明确的 runtime event path。

## 4. Execution Semantics

当前 GPIO proposed contract 暂时只记录语义边界，不冻结 API。

候选执行语义：

- `GpioInput` read 可以是同步立即完成
- `GpioOutput` write 可以是同步立即完成
- `GpioEdgeSource` 只表示可投递 edge，不表示调用方可在 ISR 完整消费事件

当前不承诺：

- reentrant
- blocking wait
- polling loop
- debounce timing
- managed time / replay 可控制
- edge callback 可以执行任意用户逻辑

如果未来需要 debounce 或 repeat，必须通过 `input.service` / sampler / timebase 进入，而不是塞进基础 GPIO contract。

## 5. Error Semantics

GPIO 公共错误语言尚未冻结。

candidate taxonomy 至少应考虑：

- `unsupported`
- `invalid_pin`
- `direction_mismatch`
- `not_configured`
- `target_detached`
- `policy_violation`
- `io_fault`
- `unknown`

平台错误可以更具体，但公共 caller 不应只收到 `false`、裸整数或 vendor status。

在 `experimental` 前，不应为了单一 HAL backend 草率扩大错误 taxonomy。

## 6. Facts

GPIO proposed contract 未来至少需要能投影下面 facts：

- `gpio.controller`
- `gpio.pin`
- `gpio.input`
- `gpio.output`
- `gpio.edge_source`
- `pinmux`
- `pull`
- `irq.line`
- `power.domain`
- `gpio.backend`
- `gpio.evidence`

这些 facts 在 v0 不做构建期执法。

它们应先服务：

- admission record
- artifact report
- evidence sample
- explain / unresolved binding 入口

## 7. 与 input.service 的关系

GPIO contract 不应吞掉输入语义。

推荐边界：

```text
GpioInput / GpioEdgeSource
  -> raw sampling / edge event
  -> input.service / sampler
  -> RawInputEvent
  -> UI/domain intent
```

`input.service` 可以负责：

- 去抖
- 时间戳
- 采样聚合
- repeat
- raw input event

UI/domain 可以负责：

- click
- long press
- focus navigation
- action intent

基础 GPIO contract 只负责 pin-level capability，不负责解释“这个输入意味着什么”。

## 8. Evidence Gaps

当前缺口明确保留：

- 没有 GPIO mock backend
- 没有 HAL adapter backend
- 没有准真实 GPIO driver / component evidence
- 没有 no-hardware smoke
- 没有 `system_compiler.fact_evidence/v0` sidecar
- 没有 board/probe/bringup evidence

现有 `hal_gpio` 只能作为 controller-facing HAL 经验参考。
它不能证明 GPIO device contract 已经 experimental。

## 9. Non-goals

当前阶段明确不做：

- 不新增 `Modules/io/device/io.device_gpio.cppm`
- 不修改 `hal_gpio`
- 不修改 `input.service`
- 不重构 input sampler
- 不宣布 GPIO contract 为 `experimental`
- 不把 debounce 放入基础 pin contract
- 不把 UI input intent 放入 GPIO contract
- 不承诺 ISR callback 可以运行任意逻辑
- 不把 pin 直接做成万能对象

## 10. Next Steps

最值当的下一步是：

1. 保持本文件为 `proposed` card。
2. 先按 [`../system/gpio_device_input_output_edge_readiness_checklist_v0.md`](../system/gpio_device_input_output_edge_readiness_checklist_v0.md) 收拢 producer / source / subject / facts / evidence 语义，但不急着写代码。
3. 再设计 `GpioInput / GpioOutput / GpioEdgeSource` 的职责卡，不急着写代码。
4. 选择一个小型 evidence 目标，例如 LED output、button input、edge counter。
5. 再决定 mock 是否先支持 level script，还是先支持 edge script。
6. 与 [`device_contract_admission_matrix_v0.md`](device_contract_admission_matrix_v0.md) 同步准入状态。

在这些完成前，GPIO 仍保持 `proposed`。
