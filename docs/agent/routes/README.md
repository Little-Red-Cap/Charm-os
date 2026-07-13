# Agent Routes

> status: `supporting`
>
> Route 是根 [`AGENTS.md`](../../../AGENTS.md) 的第二跳，只选择当前任务需要的规则、skill 和专题契约。

| 任务 | Route |
|---|---|
| 代码审查 | [`review.md`](review.md) |
| 代码生成 | [`codegen.md`](codegen.md) |
| 架构与能力归属 | [`architecture.md`](architecture.md) |
| 文档维护 | [`docs.md`](docs.md) |
| UTF-8 与乱码 | [`utf8.md`](utf8.md) |
| init.graph 与装配 | [`init-graph.md`](init-graph.md) |
| IO contract | [`io.md`](io.md) |
| Capability map / 归属 | [`capability.md`](capability.md) |
| Block device / storage | [`block-device.md`](block-device.md) |
| CMake 与构建 | [`build.md`](build.md) |

一次只进入与任务直接相关的 route。任务超出卡片范围时再读取其指向的 skill、workflow 或 contract，
不要预加载整个 `docs/agent/`。
