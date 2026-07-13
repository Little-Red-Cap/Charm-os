# IO 文档入口

IO 的行为边界由以下契约定义：

1. [`io_channel_contract.md`](io_channel_contract.md)
2. [`io_reactor_contract.md`](io_reactor_contract.md)
3. [`io_registry_contract.md`](io_registry_contract.md)

[`io_layering_overview.md`](io_layering_overview.md) 只说明这些 primitive 与平台、
driver、service 和 domain 的依赖位置，不覆盖契约正文。

## 按任务进入

| 任务 | 入口 |
|---|---|
| Channel/Reactor/Registry 行为 | 上述三份 contract |
| IO 与 driver/device 分工 | [`../architecture/driver_model.md`](../architecture/driver_model.md) |
| block device | [`../agent/routes/block-device.md`](../agent/routes/block-device.md) |
| 输入分层 | [`../input/README.md`](../input/README.md) |
| AT parser/session | [`at_runtime_contract.md`](at_runtime_contract.md) |
| 网络 socket v0 | [`net_socket_v0_contract.md`](net_socket_v0_contract.md) |
| 网络双表面设计 | [`net_stack_dual_surface_design.md`](net_stack_dual_surface_design.md) |

网络 tasklist、review 和 checklist 是阶段记录，不高于现行 contract。完整历史材料
位于 [`../archive/net-stack-v0/README.md`](../archive/net-stack-v0/README.md)。

当文档与实现冲突时，以 `Modules/io/channel`、`Modules/io/reactor`、
`Modules/io/registry` 和实际装配代码为准，并修正文档。
