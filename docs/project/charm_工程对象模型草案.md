# Charm 工程对象模型摘要

> **文档状态：`exploration`（未冻结）**

完整讨论见 [`../archive/project-proposals-v0/charm_工程对象模型草案.md`](../archive/project-proposals-v0/charm_工程对象模型草案.md)。它不能覆盖 Constitution、现行契约或 CMake 事实。

## 保留结论

草案试图用 `Product`、`Platform`、`Board`、`Scenario`、`Variant`、`Bundle`、`Runtime`、`Workflow` 和依赖范围描述真实项目的组合压力。

## 当前状态

这是一组项目建模候选，不是已经实现的对象注册表或构建 API。与工程变体、Bundle、构建升级和复杂场景提案存在明显重叠，不能让多份草案同时充当总模型。

## 重新推进条件

先明确唯一消费方和最小输出，使用当前代码与构建系统验证；只有当现有 target/preset/目录约定无法表达该需求时，才新增一个项目层对象。
