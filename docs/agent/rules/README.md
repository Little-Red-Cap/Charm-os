# Rules 路由

> status: `supporting`
>
> Rules 是跨任务操作约束，不定义 Charm Core 或专题接口。任务先由
> [`route card`](../routes/README.md) 选择需要的规则，不要一次加载全部文件。

| 问题 | Rule |
|---|---|
| 协作、澄清和变更隔离 | [`collaboration.md`](collaboration.md) |
| 嵌入式 C++、资源和执行上下文 | [`embedded-modern-cpp.md`](embedded-modern-cpp.md) |
| Core 准入、分层和工程边界 | [`charm-architecture.md`](charm-architecture.md) |

规则不能覆盖根 [`AGENTS.md`](../../../AGENTS.md)、[`CONSTITUTION.md`](../../../CONSTITUTION.md)
或专题 contract。事实判断仍以源码、CMake、真实 target 和当次验证为准。
