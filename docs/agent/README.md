# Charm Agent 文档入口

> status: `supporting`
>
> 根 [`AGENTS.md`](../../AGENTS.md) 是第一入口和最高操作约定。本页只说明第二跳文档的职责。

```text
AGENTS.md -> route -> relevant rules/contract -> skill -> optional template/glossary
```

| 入口 | 职责 |
|---|---|
| [`routes/`](routes/README.md) | 按任务选择最短阅读路径 |
| [`rules/`](rules/README.md) | 跨任务操作和工程约束 |
| [`skills/`](skills/README.md) | 某类任务的检查方法 |
| [`templates/`](templates/) | 可选输出骨架 |
| [`glossary.md`](glossary.md) | Agent 文档中的局部术语定位 |

## 使用规则

- 一次只读取当前任务需要的 route 和下一跳，不预加载整个目录。
- Route 不复制规则，skill 不覆盖规则，template 不携带判断规则。
- 源码、CMake、真实 target 和当次验证仍是实现事实来源。
- 文件移动或删除后同步更新直接入口和相对链接。

早期指导体系的退出原因见
[`../archive/agent-guidance-v0/README.md`](../archive/agent-guidance-v0/README.md)。
