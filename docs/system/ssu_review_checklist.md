# SSU 评审清单（最小机制化）

本清单用于把 `submit discipline` 从文档原则变成默认评审动作。

## 触发条件

当 PR 涉及以下任一内容时，必须执行本清单：

- 新增/修改 task 唤醒路径
- 新增/修改 pump/resubmit 路径
- 新增/修改 run_loop phase step
- 新增/修改 ISR defer/notify/drain 路径
- 新增/修改 scheduler submit 调用（post/io_ready/demand）

## 必答项

1. 该路径映射到哪类 submit？
   - `event-submit` / `io-ready-submit` / `demand-submit`
2. 为什么是这个 submit，而不是其他 submit？
3. 执行域是什么？
   - `task_only` / `isr_safe` / `mixed`
4. 是否可能阻塞？
   - `non_blocking` / `may_block`
5. 是否有预算/单步语义？
   - `budgeted` / `single_step`
6. 有无 resubmit？触发条件是什么？
7. 是否引入临时旁路？若有，回收条件与路径是什么？

## RunLoop 专项

凡是新增/修改 `RunLoop::add_step(...)`：

- 必须显式填写 `submit_projection`
- 默认不允许 `unclassified`

## 最小落地要求

- PR 描述必须填写 `.github/PULL_REQUEST_TEMPLATE.md` 中的 SSU 段落
- 评审人必须在 review 中明确“submit 映射结论”

## 当前范围

这是 Phase 2 的最小机制化，不包含自动化阻断。
后续可再升级为 CI/lint 检查。
