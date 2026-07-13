# Charm Agent Glossary

> status: `supporting`
>
> 本页只提供 Agent 文档中的术语定位，不定义 Charm Core 或公共 API。源码、CMake 和专题契约
> 才是实现事实来源。

## Agent 文档

- **Agent**：执行协作任务的 AI 协作者。
- **Rules**：跨任务生效的操作和工程约束。
- **Skills**：面向具体任务的检查和工作流程。
- **Prompt**：一次任务的输入、约束和输出要求。
- **Session**：当前对话中的任务、选择和未决事项。

## 实现名词

- **`init.graph`**：系统装配和初始化顺序的输入/表示。具体语义以
  [`init_graph_contract.md`](../system/init_graph_contract.md) 和装配代码为准。
- **`CoreSystemChain`**：`Modules/system/init` 中的局部装配实现，不是所有系统对象的共同基类。
- **`io::Channel`**：`io.channel` 提供的字节读写对象；它不自动定义协议 framing、设备所有权或
  全局能力发现。
- **`io.reactor`**：事件驱动 IO 的实现模块；调用方仍需遵守对应 IO contract 的上下文、阻塞和错误语义。
- **`io.registry`**：IO endpoint 的注册和查找入口；它不等于全局 Provider Registry，也不授予能力
  名称公共契约身份。
- **`util::Errc` / `util::Result`**：当前代码使用的结果和错误投影；具体错误集合与接口边界由各专题契约定义。
- **non-blocking**：一种行为约束，通常表示无资源时返回可处理错误，例如 `Errc::would_block`；
  不能仅凭函数名或元数据推断成立。

## 不作为全局术语

`BoardChain`、`extra nodes` 和通用 `RuntimeContext` 不在 Charm glossary 中定义。它们若出现在
某个项目、示例或平台实现中，应按局部源码和专题文档解释，不能据此推导统一架构模型。

涉及 Core 准入时先读 [`CONSTITUTION.md`](../../CONSTITUTION.md)；涉及实现状态时不要以本页替代源码、
CMake、测试或板级证据。
