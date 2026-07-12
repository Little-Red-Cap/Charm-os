# `usb_storage_bundle` 设计摘要

> **文档状态：`exploration`（未冻结）**

完整讨论见 [`../archive/project-proposals-v0/usb_storage_bundle_设计草案.md`](../archive/project-proposals-v0/usb_storage_bundle_设计草案.md)。它是 `Bundle` 总提案的具体样例，不是当前 USB 或存储契约。

## 保留结论

USB MSC 场景会同时涉及 block device、USB 初始化、DCD adapter、descriptor、读写策略和 ready hook；这些部件的组合容易重复，适合作为未来验证 Bundle 是否有价值的候选样本。

## 当前状态

未证明存在可复用的 `usb_storage_bundle` 配置、生成器或跨平台运行时对象。当前 Player、USB 和 storage 实现不可由本草案自动推断出统一边界。

## 重新推进条件

先选一个已验证 MSC 场景，明确输入、生成 target、资源所有权和失败输出；用至少两个环境复测后再决定是否提取 Bundle。
