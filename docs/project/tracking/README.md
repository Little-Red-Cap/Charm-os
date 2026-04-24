# 推进与跟踪入口

本目录收纳推进状态、认领信息、问题清单和阶段性 backlog。

这里的文档默认回答的是：

- 现在在推进什么
- 谁在认领什么
- 哪些问题已经浮出水面但还没完全收口

它们默认不直接充当长期架构契约。

## 按用途进入

- 全仓体检与收敛排期：
  [`主框架全仓审查与收敛_backlog.md`](主框架全仓审查与收敛_backlog.md)

- 当前 TODO、协作分工、认领方式：
  [`推进TODO与分工.md`](推进TODO与分工.md)

- 重构任务归属与 ownership：
  [`refactor_todo_ownership.md`](refactor_todo_ownership.md)

- Player / vivid 相关问题记录：
  [`player_issue_log.md`](player_issue_log.md)

## 使用提醒

- 先用这些文档找“正在推进什么”，不要直接拿它们当“现在系统一定如此”的证明。
- 真正的行为边界、接口约束和现行规则，仍要回到 [`../../README.md`](../../README.md)、[`../../architecture_overview.md`](../../architecture_overview.md) 和 [`../../system/README.md`](../../system/README.md)。
- 某项任务如果已经稳定沉淀为长期规则，应从 tracking 回写到入口层或契约层，而不是永远留在 backlog 里。
