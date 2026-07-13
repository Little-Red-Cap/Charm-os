# Power / Low-power 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: `Modules/system/power` 当前接口与缺口
- `source`: `power.types/core/policy/port/trace`

该子系统是局部 power policy 原型，不是跨平台低功耗契约，也不定义 Charm Core。

## 当前模型

Manager 保存单个目标状态，并将 wake source 与 clock domain 的枚举类别聚合为 bit mask。可选
policy 根据 snapshot 选择状态并应用最低状态/deep-state 约束；可选 port 执行平台动作，trace
只记录请求和转换。

状态枚举顺序参与当前 policy clamp，但不证明各平台具有相同电源状态或保留语义。Policy 只做
选择，不拥有硬件动作。当前 host 样本位于 `Examples/system/power_demo`，Win port 只返回成功，
不执行硬件操作。

## 当前缺口

- `WakeRequest.id`、`ClockRequest.id` 和 `ClockRequest.enable` 不参与聚合；同类别多个 request
  不能独立引用计数或撤销。
- `PortOps::enter()` 的 `bool` 结果被忽略；即使进入失败，`current` 仍会更新。
- 没有 lock、并发规则、transaction、rollback 或 wake reason。
- 没有 clock tree 验证、状态转换合法性、恢复顺序或设备 suspend/resume 编排。
- trace sink 是全局非 owning 函数表，没有生命周期和并发保护。
- 尚无 QEMU 或真实板低功耗证据。

在这些缺口关闭前，不把该 API 作为统一 power capability，也不从 `power_demo` 推导平台支持。
