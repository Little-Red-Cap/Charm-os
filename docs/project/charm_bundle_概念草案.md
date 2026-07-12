# Charm Bundle 概念摘要

> **文档状态：`exploration`（未冻结）**

完整讨论见 [`../archive/project-proposals-v0/charm_bundle_概念草案.md`](../archive/project-proposals-v0/charm_bundle_概念草案.md)。项目层草案不能改变 Core 裁决；上位规则见 [`../architecture/charm_core_contract.md`](../architecture/charm_core_contract.md)。

## 保留结论

`Bundle` 被提出用于把一组经常共同装配、共同验证的能力或节点作为项目级组合单元管理。它不应替代 `init.graph`，也不应成为 runtime、全局注册表或应用 ABI。

## 当前状态

仓库尚未证明有统一 Bundle API、schema、generator 或跨平台 consumer。`usb_storage_bundle`、`audio_output_bundle` 等名称仍是场景提案，不是现行构建目标或公共术语。

## 重新推进条件

先从一个真实重复的组合场景提取最小配置，给出 Host 与至少一个非 Host 实现、失败行为和可重复 smoke；否则继续使用现有 CMake、profile 和显式装配，不新增 Bundle 层。
