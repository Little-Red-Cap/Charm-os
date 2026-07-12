# Charm Agent 文档入口

> **文档状态：`supporting`**

根 [`AGENTS.md`](../../AGENTS.md) 是 Agent 的第一入口和最高操作约定。本页只解释第二跳文档之间的关系，
不重复任务路由、工程规则或输出格式。

## 文档层级

| 目录/文件 | 职责 | 不负责 |
|---|---|---|
| [`routes/`](routes/README.md) | 按任务给出最短阅读路径 | 重复规则正文 |
| [`rules/`](rules/README.md) | 长期、跨任务的操作与工程边界 | 逐步任务教程 |
| [`skills/`](skills/README.md) | 某类任务的检查重点和方法 | 覆盖 rules |
| [`workflows/`](workflows/) | 执行顺序与停点 | 定义项目语义 |
| [`templates/`](templates/) | 可选输出骨架 | 承载判断规则 |
| [`glossary.md`](glossary.md) | 术语定位和下一跳 | 替代源码或 contract |

优先级固定为：

```text
AGENTS.md -> route -> relevant rules -> skill/workflow -> optional template/glossary
```

更深目录存在 `AGENTS.md` 时，由更深层约定覆盖对应范围。

## 加载策略

1. 从根 `AGENTS.md` 识别任务类型；
2. 只读取一个直接相关的 route；
3. 按 route 加载必要 rules 与专题 contract；
4. 任务确实需要时再读取 skill、workflow、template 或 glossary；
5. 任务改变类型后再切换 route，不预加载整个 `docs/agent/`。

这样做是为了减少上下文噪声，不是为了跳过源码核对。Charm 中的事实仍按根 `AGENTS.md` 规定的信任顺序判断。

## 目录入口

- [`routes/README.md`](routes/README.md)：可用任务卡及第二跳说明；
- [`rules/collaboration.md`](rules/collaboration.md)：协作与沟通；
- [`rules/embedded-modern-cpp.md`](rules/embedded-modern-cpp.md)：嵌入式 C++ 约束；
- [`rules/charm-architecture.md`](rules/charm-architecture.md)：架构与工程纪律；
- [`skills/README.md`](skills/README.md)：技能入口；
- [`workflows/`](workflows/)：review、codegen、architecture 流程；
- [`templates/`](templates/)：对应输出模板。

具体任务不要从本页猜测路径；以根 `AGENTS.md` 和 [`routes/`](routes/README.md) 为准。

## 维护规则

- 一条规则只保留一个第一解释位置，其它文档使用链接；
- route 只给最短下一跳，不展开专题背景；
- skill 不复制 rule，workflow 不复制 skill，template 不携带项目规则；
- 文件新增、移动或删除后同步修正入口和相对链接；
- 过期场景清单、目录树快照和阶段性协作口号移入 archive；
- 仓库文档是版本维护源，本机 Agent 配置是否同步由具体工具和个人环境决定。

根 [`config.toml`](../../config.toml) 可以声明本仓入口，但不是 Charm 构建或运行时依赖。

早期完整说明归档于
[`../archive/agent-guidance-v0/agent_readme_legacy.md`](../archive/agent-guidance-v0/agent_readme_legacy.md)。
