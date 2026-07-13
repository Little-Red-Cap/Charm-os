# USB 示例入口

## 文档状态

- `status`: `supporting`
- `scope`: USB mock、replay、Host runtime 与 board-log fixture 路由
- `authority`: 各目录 CMake/source 与 [`USB contracts`](../../docs/usb/README.md)

## 证据域

| 域 | 证明范围 |
|---|---|
| Host mock/replay | descriptor、EP0、class state、composite 与 trace replay |
| real board | DCD glue、IRQ/callback、endpoint timing 与 host enumeration |
| QEMU | kernel/system 配合，不作为 USB device 主证据 |

三类证据不能互相替代。

## 按范围定位

| 范围 | 目录 pattern |
|---|---|
| CDC / MSC / composite | `usb_*_mock_smoke`、`usb_*_replay_smoke` |
| manifest / board log | `usb_*_manifest_smoke`、`usb_*_boardlog_*` |
| Host adapter | `usb_host_harness_smoke` |
| runtime capability | `usb_host_runtime_*` |
| block-device integration | [`usb_msc_block_demo`](usb_msc_block_demo/README.md) |

target、case 与 token 由各目录 CMake/source 定义，本页不维护完整清单。

## Host Runtime

`usb.host.core` 把 Host adapter 投影为 `device::Bus`；`usb.host.runtime` 提供 runtime bus；
`runtime_block/channel` 通过 stable slot 导出 MSC/CDC；`runtime_manager` 编排 scan、remove、rediscover
与 reset。共享 fixture 位于 `support/`。

Runtime sidecar 是场景结束快照。multi fixture 在最后执行 remove/forget/unexport，因此空
`published_capabilities` 或 publish/export `missing` 可以是预期终态；必须结合 transition 与源码断言，
不能单凭最终计数判定失败。

## Runner

[`usb_native_smoke.ps1`](../../scripts/usb_native_smoke.ps1) 是 Host 聚合入口。默认复用既有 build
directory；`-ConfigureOnly` 只配置，`-Clean` 仅在明确需要丢弃缓存时使用。MCU/real-board 使用各自
toolchain 与 evidence runner。
