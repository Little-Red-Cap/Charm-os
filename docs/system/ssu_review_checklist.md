# SSU 执行路径评审清单

## 文档状态

- `status`: `supporting`
- `scope`: kernel task 元数据、Scheduler submit 与 RunLoop projection 的代码评审
- `contract`: [`ssu_contract.md`](ssu_contract.md)
- `PR surface`: [`.github/PULL_REQUEST_TEMPLATE.md`](../../.github/PULL_REQUEST_TEMPLATE.md)

## 何时执行

变更涉及以下任一内容时使用本清单：

- `TaskRegistry` task 或 `ssu_meta()`；
- task 唤醒、pump、drain、resubmit；
- ISR defer/notify；
- `post*`、`post_io_ready*`、`post_demand*`；
- `RunLoop::add_step()` 或 `SubmitProjection`。

## 必答项

1. 执行对象是否进入 `TaskRegistry`；严格模式是否要求 `ssu_meta()`？
2. 元数据是否使用代码中的准确枚举：`isr_only/task_only/anywhere`、
   `event/io_ready/timer/frame/demand`、`single_step/budgeted`、
   `non_blocking/may_block`？
3. 提交属于 `event-submit`、`io-ready-submit` 还是 `demand-submit`；为什么？
4. ISR 和 task context 的边界在哪里？
5. 单步或预算边界是什么；做不完时如何 resubmit？
6. 代码是否可能阻塞、busy-spin、sleep 或在内部等待 timeout？
7. 观测面能否区分这条路径；现有统计是否足够？
8. 元数据是否只是声明；哪些行为仍依赖人工评审或测试？

## Submit 选择

- `event-submit`：离散消息、生命周期事件和普通状态推进。
- `io-ready-submit`：IO 已 ready，重处理在 task context 中完成。
- `demand-submit`：下游明确需要数据或需要继续推进。
- timer/frame 可以使用对应 `TriggerKind`；不要因此虚构不存在的 Scheduler 入口。

若三类均不适用，不要通过改名掩盖差异。记录：

- 旁路原因；
- 影响范围；
- 退出条件；
- 计划回收到的入口，或证明该分类需要修改的证据。

## RunLoop

- `RunLoop::add_step()` 必须显式传入 `SubmitProjection`。
- `add_scheduler_step()` 和 `add_reactor_step()` 的固定映射应与实现一致。
- projection 当前只用于审计，不得声称它已改变 Scheduler 行为。

## 本地检查

```powershell
scripts/ssu_submit_gate.ps1 -Staged
```

该脚本是关键词启发式检查：它只能发现部分 submit/RunLoop 相关 diff，并要求同时更新本清单。
脚本通过不代表行为正确，也不能替代构建、测试和人工评审。
