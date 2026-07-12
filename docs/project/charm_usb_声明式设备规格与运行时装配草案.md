# Charm USB 声明式装配摘要

> **文档状态：`exploration`（未冻结）**

完整讨论见 [`../archive/project-proposals-v0/charm_usb_声明式设备规格与运行时装配草案.md`](../archive/project-proposals-v0/charm_usb_声明式设备规格与运行时装配草案.md)。当前 USB 代码和板级验证是事实来源。

## 保留结论

草案提出用设备规格、功能配置和运行时 binding 减少重复 USB glue，并保留专家入口；它特别关注 MSC、Audio、descriptor、DCD adapter 和 ready hook 的边界。

## 当前状态

未证明存在统一的 `UsbDeviceSpec`、`UsbFunctionSpec`、`UsbRuntimeBinding` schema 或 generator。现有 USB class、board glue、service 和脚本不能自动合并成该模型。

## 重新推进条件

选一个已运行的 USB 场景，先固定输入、生成结果和失败诊断，再以 Host/QEMU/real board 中至少两个环境验证；在此之前不要把声明式 USB 对象加入 Core 或默认构建入口。
