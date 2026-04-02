# SSU RunLoop/Phase 提交来源审计（Phase 2）

## 目标

在不改变现有调度行为的前提下，把 run_loop / phase 路径的 submit 语义做成可审计事实，
并把“提交来源声明”从软标签提升为注册接口的一部分。

## 当前规则

- `add_scheduler_step(...)` 固定映射到 `SubmitProjection::event`
- `add_reactor_step(...)` 固定映射到 `SubmitProjection::io_ready`
- 直接 `loop.add_step(...)` 必须显式传入 `SubmitProjection`
- run_loop 可通过 `format_audit_json(...)` 导出阶段/submit/name 审计结果

> 说明：`SubmitProjection` 目前仍服务于审计与主线收口，不直接改变 scheduler 行为。

## 代码落点

- `Modules/system/loop/system_run_loop.cppm`
  - `enum class SubmitProjection`
  - `LoopStep.submit_projection`
  - `to_text(LoopPhase)` / `to_text(SubmitProjection)`
  - `RunLoop::format_audit_json(...)`

## 已知调用点审计

### 1) `Examples/project/player/stn32h747_HQZY/CM7/app/main.cpp`

- `add_reactor_step(..., LoopPhase::io, ...)` -> `io-ready-submit`
- `add_scheduler_step(..., LoopPhase::update, ...)` -> `event-submit`

### 2) `Examples/project/player/win/main.cpp`

- `loop.add_step(LoopPhase::io, SubmitProjection::event, ...)`
- `loop.add_step(LoopPhase::update, SubmitProjection::event, ...)`
- `loop.add_step(LoopPhase::render, SubmitProjection::event, ...)`

## 当前收益

1. run_loop 新增 step 不再允许“漏填 submit 来源”。
2. phase 推进路径已从字符串标签升级为强类型声明。
3. 后续若要把 phase 统计接入 observability，已有统一导出入口。

## 下一步

1. 选一个真实 loop 样板导出 `format_audit_json(...)` 结果，形成审计样例。
2. 继续把更多 phase/loop 驱动路径投影到 `event / io_ready / demand` 三分法。
3. 评估是否需要新增 `SubmitProjection::demand` 的真实 run_loop 样板，而不是只停留在枚举层。
## 审计输出样例

`Examples/project/player/win/main.cpp` 已接入：

- `loop.format_audit_json(...)`
- 启动日志输出 `[runloop.audit] ...`

示例（简化）：

```json
[{"index":0,"phase":"io","submit":"event-submit","name":"player_io"},{"index":1,"phase":"update","submit":"event-submit","name":"player_update"},{"index":2,"phase":"render","submit":"event-submit","name":"player_render"}]
```
