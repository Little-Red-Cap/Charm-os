# Project Proposals v0 归档

本目录保存 Foundation Runtime、USB 声明式装配、工程对象、构建升级、Config Module 和
USB storage bundle 的独立草案。

这些材料保留了真实项目压力、候选对象和未决取舍，但没有共同的实现、CMake 入口或跨环境证据。
现行目录只保留被 Player 落地稿引用的 `charm_工程对象模型草案.md` 状态桥接；其它提案不再维护重复摘要。

默认入口：

- [`../../README.md`](../../README.md)
- [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)
- [`../../project/README.md`](../../project/README.md)

如果重新推进某个提案，先核对当前源码、构建入口和测试，再决定是补入已有契约、建立独立实验，还是继续归档。

保留文件：

- [`charm_工程对象模型草案.md`](charm_工程对象模型草案.md)
- [`charm_构建系统升级方向草案.md`](charm_构建系统升级方向草案.md)
- [`charm_foundation_runtime_与统一应用入口模型草案.md`](charm_foundation_runtime_与统一应用入口模型草案.md)
- [`charm_usb_声明式设备规格与运行时装配草案.md`](charm_usb_声明式设备规格与运行时装配草案.md)
- [`usb_storage_bundle_设计草案.md`](usb_storage_bundle_设计草案.md)
- [`config_module_draft.md`](config_module_draft.md)

通用 Bundle 概念、复杂场景总括提案和工程变体模型已删除：前两者的机制由具体提案承接，工程
变体已由工程对象模型取代。需要追溯原文时使用 Git 历史。
