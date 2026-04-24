# Charm Agent Glossary

本文件用于统一 Charm 项目中的常用术语，避免协作时语义漂移。

## 1. Agent
在本项目语境中，Agent 指执行协作任务的 AI 协作者：
- 受 rules 约束
- 按 skills 选择工作流
- 使用统一模板组织输出

## 2. Rules
Rules 是跨任务生效的硬约束，回答“在 Charm 里必须遵守什么”。

## 3. Skills
Skills 是面向具体任务的流程模板，回答“遇到这类任务应如何做”。

## 4. Prompt
Prompt 是当前任务的输入说明，回答“这次要做什么”。

## 5. Session
Session 是当前对话上下文（任务、约束、选择、未决点）。

## 6. init.graph
统一的能力装配与初始化图，禁止入口手写顺序。

## 7. CoreSystemChain
系统底座能力装配链（registry/reactor/eda/pump/clock）。

## 8. BoardChain
板级能力装配链（irq/hal/driver/io.*）。

## 9. extra nodes
仅用于服务/应用/实验节点的额外链路，禁止底座能力进入。

## 10. io::Channel
统一非阻塞字节通道，read/write 禁止 Ok(0)。

## 11. io.reactor
统一事件驱动 IO，协议层禁止 busy-spin/自带超时。

## 12. io.registry
统一能力发现入口，禁止隐式全局默认通道。

## 13. RuntimeContext
统一能力注入容器，禁止模块偷取全局对象。

## 14. util::Errc / util::Result
统一错误模型与结果类型，禁止自建错误枚举导致语义分裂。

## 15. Non-blocking
不阻塞、不 busy-spin、无资源时返回可处理错误（如 Errc::would_block）。
