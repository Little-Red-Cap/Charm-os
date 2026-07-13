# Project Proposals v0 归档

本目录保存早期诊断、USB 声明式装配、工程对象、构建升级、Config Module 和
USB storage bundle 的独立草案。

这些材料保留了真实项目压力、候选对象和未决取舍，但没有共同的实现、CMake 入口或跨环境证据。
Player 落地稿引用的工程对象状态桥接保留在
[`../../project/charm_工程对象模型草案.md`](../../project/charm_工程对象模型草案.md)；其它提案不再维护重复摘要。

默认入口：

- [`../../README.md`](../../README.md)
- [`../../architecture/charm_core_contract.md`](../../architecture/charm_core_contract.md)
- [`../../project/README.md`](../../project/README.md)

如果重新推进某个提案，先核对当前源码、构建入口和测试，再决定是补入已有契约、建立独立实验，还是继续归档。

保留文件：

- [`build_model_retained_notes.md`](build_model_retained_notes.md)：从早期构建升级草案保留显式 target、
  BSP source ownership、preset/workflow 和迁移失败边界。
- [`early_diagnostics_retained_notes.md`](early_diagnostics_retained_notes.md)：从 Foundation Runtime 草案
  中保留复杂装配前诊断、应用入口与平台 startup 分离、early/full sink 交接问题。
- [`usb_declarative_retained_notes.md`](usb_declarative_retained_notes.md)：从早期声明式 USB 草案保留
  spec/runtime binding、generator 顺序、专家边界、MSC storage 组合与协议调试取舍。
- [`config_module_draft.md`](config_module_draft.md)

通用 Bundle 概念、复杂场景总括提案和工程变体模型已删除：前两者的机制由具体提案承接，工程
变体已由工程对象模型取代。需要追溯原文时使用 Git 历史。

声明式 USB 全文也已压缩；旧伪 API、场景文件映射、MSC 迁移排期和重复示例由当前 USB source、
overview 与 Git 历史替代。

`usb_storage_bundle` 草案已并入同一保留笔记；已删除的通用 Bundle 名词、Player/HQZY 迁移步骤和
旧 profile/runtime 文件映射不再保留。

Foundation Runtime 全文已删除；该宽泛名词未通过 Core 准入，早期诊断问题已收窄为局部平台边界。

构建升级全文已压缩；候选 Package 名词、Player 场景清单、旧目录布局和阶段路线不再保留。

工程对象模型全文已删除；Product/Board/Profile/Execution Environment 等候选的当前裁决只保留在
项目层状态桥接，不再维护九对象总模型和伪 CMake API。
