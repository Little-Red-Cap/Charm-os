# Charm 构建系统升级摘要

> **文档状态：`exploration`（未冻结）**

完整讨论见 [`../archive/project-proposals-v0/charm_构建系统升级方向草案.md`](../archive/project-proposals-v0/charm_构建系统升级方向草案.md)。构建行为以当前 CMake、preset 和 target 为准。

## 保留问题清单

- 单一工程入口难以承载多个 profile、板卡和验证场景；
- IDE、命令行、脚本和板级资源的边界容易重复；
- 构建配置、运行时配置和产品场景不应继续混在一个主 CMake 文件中。

## 当前状态

`Board Package`、`Profile Spec`、`Runtime Package`、`Bundle Package`、`Workflow Target` 仍是候选构建层概念，没有统一实现。不要以草案模型重写现有构建入口。

## 重新推进条件

从一个现有多目标构建痛点开始，先用 target/preset 或独立工具解决，并记录构建时间、依赖和失败诊断；只有重复证据足够时再提取公共层。
