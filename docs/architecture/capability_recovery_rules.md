# 能力回收执行规则（工程化模板）

目标：把“能力回收”从口号变成稳定流程，确保耦合发生但不失控。

## 硬规则（必须遵守）

- trace_core 只写入/上报，禁止格式化与策略逻辑。
- 只回收“存储模型”，禁止把领域语义塞回 Foundation。
- util.units 只表达量纲，禁止提供时间源/调度语义。
- Domain 事件队列禁止阻塞/重试/睡眠（满了直接丢弃）。
- 新 board/app 代码默认必须优先走 `DefaultConsolePath`，不得先发明第二套运行期打印 API。
- 直连 port/UART 的输出只允许作为 `EarlyConsole` 例外路径存在，不得无界外溢到运行期系统壳与共享 service 层。
- 任何 board service 若要进入共享协调层，必须先满足 `ServiceSnapshotContract`：只读、无副作用、可重复读取、可串口打印。
- “文档里推荐”不算完成回收；只有进入 `BoardCaps / BringupMinimal / app template / service contract` 的路径，才算默认路径。

## 术语约定（当前固定）

- `DefaultConsolePath`
  `BoardCaps/ConsoleCaps -> io.console0 -> out.channel_sink -> out.api/out.logger`
- `EarlyConsole`
  允许直接绑定 port/UART 的早期例外路径，仅用于 pre-graph、fault、极早期生存证据
- `ServiceSnapshotContract`
  board service 进入共享协调层前必须提供的只读状态面
- `EvidenceRig`
  用于暴露 Charm 真实落地问题的板级实验台；它提供证据，但不自动等于通约架构契约

## 三段式回收流程（执行模板）

每一条能力回收，必须同时满足：

1) 使用层变化
- 领域模块只引用新能力。
- 旧 API 保留，但禁止新增调用。

2) 依赖验证
- 非法 import 必须编译期失败。
- 不允许通过 forward declare 绕过。

3) 最小回归
- 编译通过 + 一个行为点验证。
- 不要求完整测试，但必须可复现。

## Board / Bringup 特别规则

### 1. 默认路径优先，例外路径命名

- 只要系统已经建立 `BoardCaps/ConsoleCaps` 与 `io.console0`，运行期输出默认应转入 `DefaultConsolePath`。
- `EarlyConsole` 必须被当作例外路径显式命名，而不是继续伪装成普通业务输出接口。

### 2. 系统协调层只消费 snapshot，不接管重 runtime

- 类似 `system_probe` 的共享系统层默认只消费 `snapshot/status`。
- 在没有稳定 `ServiceSnapshotContract` 前，不允许把 audio/display/net/storage 的重 runtime 强行拖进系统协调层。

### 3. EvidenceRig 先提炼结论，再推广实现

- 真实板级工程优先作为 `EvidenceRig` 使用，用来暴露默认路径缺失、接缝阻力与模板不足。
- 不允许把某块板子的局部 workaround 直接宣传成 Charm 通约层标准答案。

## 推荐执行顺序（低风险演习）

1) Board / Bringup：统一 `DefaultConsolePath` 与 `EarlyConsole`
2) Board / Coordination：把共享 shell/status 收束到 `ServiceSnapshotContract`
3) UI/Ink：sprintf -> out.format
4) UI/Vivid：日志 -> out.logger/trace_core
5) UI/Vivid：容器/池 -> core/service 固定容量

## 回收记录字段（建议统一格式）

- 模块：
- 能力：
- 新依赖：
- 旧接口状态（保留/禁止新增/已删除）：
- 编译验证：
- 行为验证：
- 风险备注：

---

备注：此文档只定义流程与红线；真实板级落地如何暴露这些问题，以及为什么优先从输出链与 bringup 落点收口，见：

- `docs/architecture/real_board_landing_gap_audit_v0.md`
- `docs/architecture/capability_recovery_matrix.md`
