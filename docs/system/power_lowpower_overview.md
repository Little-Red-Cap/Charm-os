# Power / Low-power 实现状态

## 文档状态

- `status`: `supporting`
- `scope`: `Modules/system/power` 当前接口与缺口
- `source`: `power.types/core/policy/port/trace`

该子系统是局部 power policy 原型，不是跨平台低功耗契约，也不定义 Charm Core。

## 类型与职责

| module | 当前内容 |
|---|---|
| `power.types` | `State`、`WakeSource`、`ClockDomain` 与 request 结构 |
| `power.policy` | `Constraints`、`PolicySnapshot` 与虚接口 `Policy` |
| `power.core` | 聚合请求、选择状态并调用 port 的 `Manager` |
| `power.port` | `enter(State) -> bool`、`exit(State)` 函数表 |
| `power.trace` | request/enter/exit/source/domain 事件 sink |

`State` 枚举为 `active`、`idle`、`sleep`、`deep_sleep`、`stop`、`standby`。枚举顺序被当前
policy clamp 使用，但不证明所有平台具有这些状态或相同保留语义。

## `Manager` 行为

1. `request()` 保存单个目标状态并记录 trace。
2. wake source 与 clock domain 按枚举类别写入 32-bit mask。
3. `decide_target()` 调用 policy；没有 policy 时直接返回请求值。
4. `Constraints::min_state` 限制枚举下界；`allow_deep=false` 将 deep/stop/standby 收敛到 sleep。
5. `enter_state()` 调用可选 port hook、更新 `current` 并记录 trace；`exit_state()` 恢复 active。

`Policy` 只选择目标，硬件动作由 `PortOps` 提供。当前 host 样本位于
`Examples/system/power_demo`，Win port 只返回成功并不执行硬件操作。

## 当前缺口

- `WakeRequest.id`、`ClockRequest.id` 和 `ClockRequest.enable` 不参与聚合；同类别多个 request
  不能独立引用计数或撤销。
- `PortOps::enter()` 的 `bool` 结果被忽略；即使进入失败，`current` 仍会更新。
- 没有 lock、并发规则、transaction、rollback 或 wake reason。
- 没有 clock tree 验证、状态转换合法性、恢复顺序或设备 suspend/resume 编排。
- trace sink 是全局非 owning 函数表，没有生命周期和并发保护。
- 尚无 QEMU 或真实板低功耗证据。

在这些缺口关闭前，不把该 API 作为统一 power capability，也不从 `power_demo` 推导平台支持。
