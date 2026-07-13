## 摘要

- 改了什么：
- 为什么需要：

## 执行路径审计（按需）

涉及 task wakeup、pump/drain、loop step、deferred work、submit API 或 ISR defer 时填写：

- Submit kind：`event-submit` / `io-ready-submit` / `demand-submit` / `none`
- 选择理由：
- Execution domain：`isr_only` / `task_only` / `anywhere`
- Blocking：`non_blocking` / `may_block`
- Budget：`single_step` / `budgeted`
- Resubmit / recovery / 临时旁路退出条件：
- `RunLoop::add_step()` 的显式 `submit_projection`（如适用）：

同时核对 [`ssu_review_checklist.md`](../docs/system/ssu_review_checklist.md)，并运行：

```powershell
scripts/ssu_submit_gate.ps1 -Staged
```

## 风险与验证

- 主要风险：
- 已运行命令：
- 已验证 target / evidence domain：
- 明确 deferred：
