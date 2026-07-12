# Foundation Runtime 与统一应用入口摘要

> **文档状态：`exploration`（未冻结）**

完整讨论见 [`../archive/project-proposals-v0/charm_foundation_runtime_与统一应用入口模型草案.md`](../archive/project-proposals-v0/charm_foundation_runtime_与统一应用入口模型草案.md)。本文件不定义 Charm Core 或应用 ABI。

## 保留结论

草案试图区分基础诊断/启动能力、平台承载、系统服务和场景应用，并要求 Host 与 MCU 的应用入口尽量共享可验证语义。

## 当前状态

`Foundation Runtime` 不是当前源码中的独立公共层。现有启动、init graph、AppRuntime、平台 backend 和 resident loader 必须分别按各自实现边界理解，不能由本名称合并成一个运行时对象。

## 重新推进条件

先明确一个实际 consumer、入口 API、失败阶段和两个执行环境的等价证据；没有这些证据时，不新增 `FoundationRuntime` 基类、全局上下文或统一入口替代现有实现。
