# USB 文档入口

## 文档状态

- `status`: `supporting`
- `scope`: USB 专题路由

源码与测试优先于本目录说明。先读
[`usb_architecture_overview.md`](usb_architecture_overview.md)；执行入口见
[`Examples/usb/README.md`](../../Examples/usb/README.md)。

## 按问题进入

| 问题 | 入口 |
|---|---|
| module 分层与证据边界 | [`usb_architecture_overview.md`](usb_architecture_overview.md) |
| descriptor DSL 与 string/lang | [`usb_dsl_overview.md`](usb_dsl_overview.md)、[`usb_strings_overview.md`](usb_strings_overview.md) |
| native mock/replay | [`usb_native_mock_workflow.md`](usb_native_mock_workflow.md) |
| CDC control/data | [`usb_cdc_contract.md`](usb_cdc_contract.md) |
| boardlog/replay | [`usb_boardlog_format.md`](usb_boardlog_format.md)、[`usb_boardlog_coverage_matrix.md`](usb_boardlog_coverage_matrix.md)、[`usb_msc_trace_vocabulary.md`](usb_msc_trace_vocabulary.md) |

`contract` 只约束对应局部接口；`workflow`、`matrix` 和 `guidelines` 不自动成为架构契约。Native
fixture 不替代板级 DCD/HCD、IRQ、DMA、cache 和真实主机兼容性验证。
