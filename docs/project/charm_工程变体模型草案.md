# Charm 工程变体模型摘要

> **文档状态：`exploration`（未冻结）**

完整讨论见 [`../archive/project-proposals-v0/charm_工程变体模型草案.md`](../archive/project-proposals-v0/charm_工程变体模型草案.md)。项目变体不是 Charm Core 语义。

## 保留结论

草案尝试区分 `Platform`、`Board`、`Profile`、`Runtime`、`Bundle`、`Variant`、依赖范围和工作流，以描述同一应用跨 Host、MCU 和多硬件目标的构建差异。

## 当前状态

这些名称与当前 CMake target、preset、board package 的对应关系尚未统一；Player 例子是项目压力，不是通用模型的证明。`Platform/Board/Profile` 的现有含义仍以源码和各自入口为准。

## 重新推进条件

先从一个真实的多目标构建矩阵提取最小数据，证明它能减少重复配置且不增加新的全局注册机制，再决定是否需要模型或工具。
