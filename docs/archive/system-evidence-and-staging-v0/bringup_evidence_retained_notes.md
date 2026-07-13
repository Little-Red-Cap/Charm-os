# Bring-up Evidence 早期取舍保留笔记

> `status`: `archived`

现行状态 shape、exporter、inspector 和失败边界见
[`artifact_report_v0.md`](../../system/artifact_report_v0.md)。本文只保留 bring-up 状态解释与证据分层，
不复制字段或命令。

## 状态不可折叠

- `declared/materialized` 只表示输入声明或静态装配，不证明 capability 已发布或硬件已运行。
- `published` 表示进入可消费表面，不等于 backend 仍 attached/live，也不证明行为正确。
- `observed` 表示观察面产生记录，不证明记录完整或通过验收。
- `blocked` 必须指出缺失前置条件；`failed` 必须保留尝试阶段与原始错误。

Report 需要分别保留 publish、attach/export、runtime transition 和 operation error，不能压成单个 success
位或全局 runtime enum。

## Provenance 与证据域

结论必须追溯到 producer、execution environment、artifact 和时间/版本上下文。Project/board fact、
materialized graph、runtime sidecar、Host fixture、QEMU run 与真实板 capture 属于不同证据域，不能互相
替代。日志 token 只证明采集器看到了文本。

## Static 与 Dynamic Plane

固定板级资源和项目装配可以由 facts、profile 与 init/materialize 描述；runtime discovery、hotplug、
stable-slot attach/detach 通过 runtime observation 表达，不伪装成静态 graph node。两条平面可共享
capability name 和 evidence vocabulary，但 ownership、lifecycle 与失败语义独立。

结构 unchanged 时，runtime sidecar、publish state 或 provenance 仍可能漂移；compare 应保留这类变化，
不能伪造 node/edge diff，也不能因没有结构变化而丢弃。

Report 生成成功只证明输入可读取和投影，不证明 bring-up、消费者行为或硬件事实成立。
