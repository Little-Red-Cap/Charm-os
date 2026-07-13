# Kernel module milestone 归档

> `status`: `archived`

本目录保存早期 kernel/util 结构与 M1/M2/M3 阶段材料：

- [`kernel_util_structure_draft.md`](kernel_util_structure_draft.md)：旧目录和能力注入草案；
- [`m1_sync_spec.md`](m1_sync_spec.md)、[`m1_tests.md`](m1_tests.md)：早期 sync/IPC 语义与 checklist；
- [`m2_thread_spec.md`](m2_thread_spec.md)、[`m2_api_freeze.md`](m2_api_freeze.md)：早期 thread/blocking 设计；
- [`m3_observability_plan.md`](m3_observability_plan.md)：旧 trace/diagnostics 计划。

这些文件中的 freeze、demo 路径、CSV header 和完成状态均不约束当前实现。现行入口为
[`docs/system/README.md`](../../../system/README.md) 和 `Modules/system/kernel` 源码。
