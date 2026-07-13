# SSU 执行路径评审清单

## 文档状态

- `status`: `supporting`
- `scope`: kernel task 元数据、Scheduler submit 与 RunLoop projection 的代码评审
- `contract`: [`ssu_contract.md`](ssu_contract.md)
- `PR surface`: [`.github/PULL_REQUEST_TEMPLATE.md`](../../.github/PULL_REQUEST_TEMPLATE.md)

## 何时执行

变更涉及 `TaskRegistry`/`ssu_meta()`、task 唤醒与 resubmit、ISR defer/notify、Scheduler submit 或
`RunLoop::add_step()`/`SubmitProjection` 时使用本清单。

## 必答项

| 问题 | 要求 |
|---|---|
| registry/meta | 是否进入 `TaskRegistry`；严格模式是否要求 `ssu_meta()`；枚举是否准确 |
| submit | 属于 `event-submit`、`io-ready-submit` 还是 `demand-submit`；理由是什么 |
| context | ISR 与 task context 的边界在哪里 |
| budget | 单步或预算边界是什么；未完成时如何 resubmit |
| blocking | 是否可能阻塞、busy-spin、sleep 或等待 timeout |
| evidence | 观测面能否区分该路径；哪些行为仍依赖人工评审或测试 |

## Submit 选择

- `event-submit`：离散消息、生命周期事件或普通状态推进；
- `io-ready-submit`：IO 已 ready，重处理进入 task context；
- `demand-submit`：下游需求或继续推进；
- timer/frame 使用对应 `TriggerKind`，不虚构 Scheduler 入口。

三类均不适用时，记录旁路原因、影响范围、退出条件，以及计划回收入口或修改分类所需证据。

## RunLoop

- `RunLoop::add_step()` 显式传入 `SubmitProjection`；
- `add_scheduler_step()` 和 `add_reactor_step()` 的固定映射必须与实现一致；
- projection 只用于审计，不改变 Scheduler 行为。

## 本地检查

```powershell
scripts/ssu_submit_gate.ps1 -Staged
```

该脚本是关键词启发式检查：它只能发现部分 submit/RunLoop 相关 diff，并要求同时更新本清单。
脚本通过不代表行为正确，也不能替代构建、测试和人工评审。
