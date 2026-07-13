# Bring-up Evidence 早期取舍保留笔记

> status: `archived`
>
> scope: bring-up 解释状态、证据来源与静态/动态平面边界

现行 artifact report 的 schema、exporter、inspector 和失败边界见
[`artifact_report_v0.md`](../../system/artifact_report_v0.md)。本文不复制字段和命令。

## 状态解释

早期讨论使用六个报告状态定位 bring-up 卡点：

| 状态 | 最小含义 |
|---|---|
| `declared` | 输入侧声明了 capability、fact、node 或关系 |
| `materialized` | 声明已被规范化为当前静态装配结果 |
| `published` | capability 已进入某个可消费 registry/export surface |
| `observed` | 稳定观察面记录了状态或 transition |
| `blocked` | 动作尚未推进，因为前置条件缺失或不合法 |
| `failed` | 动作已经尝试并返回明确失败 |

这些词是 report/explain 语言，不替换模块局部状态机，也不构成全局 runtime enum。`blocked` 必须指出
缺失前置条件；`failed` 必须保留原始阶段与错误。没有原因的状态标签不能诊断问题。

## 不可折叠的状态

`published` 只表达系统可见性，不等于设备仍 attached/live，也不等于 capability 行为正确。
`observed` 只表达某观察面产生了记录，不等于记录真实、完整或通过验收。

因此报告需要按来源分别保留 publish state、attach/export state、runtime transition 和操作错误，不能
为了统一表格把它们压成单个 success 位。

## 证据来源

每条结论至少要能追溯到 producer、execution environment、artifact 和时间/版本上下文。来源包括：

- project/board 声明或 fact input；
- materialized static graph；
- registry/export runtime sidecar；
- host fixture、QEMU run 或真实板 capture。

Host fixture、QEMU 与真实板证明不同环境，不能互相替代。声明和 graph 只证明输入/装配，不证明硬件
执行；日志 token 只证明采集器看到了文本，不自动证明状态机正确。

## 静态与动态平面

片上资源和固定项目装配可以由 BoardFacts、profile、init/materialize 描述。运行期 discovery、hotplug、
stable slot attach/detach 不应被伪装成静态 graph node；它们通过 runtime observe sidecar 接入同一报告。

两条平面可以共享 capability name 和 evidence vocabulary，但 ownership、lifecycle 与失败语义保持独立。

## Compare 边界

结构未变化时，runtime sidecar、publish state 或 evidence provenance 仍可能漂移。这类变化应进入专用
comparison payload，不应伪造 node/edge diff，也不能因结构 unchanged 被丢弃。

## 不构成的承诺

- report 生成成功不证明 bring-up 成功；
- declared/materialized 不证明 published/live；
- published/observed 不证明消费者行为正确；
- 横向 case matrix 不定义统一 system model；
- evidence 工具不拥有 Capability Contract 或硬件事实。

旧 fixture catalog、DOT/JSON 样例、inspect 参数矩阵、compare 演进和路线总结已删除。
