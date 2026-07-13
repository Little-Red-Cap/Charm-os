# USB 示例入口

USB 行为契约先从 [`docs/usb/README.md`](../../docs/usb/README.md) 进入。本目录只保存 mock、replay、
host runtime 与板级日志 fixture。

## 证据域

| 域 | 证明范围 |
|---|---|
| PC mock/replay | descriptor、EP0、class state、composite 装配与 trace replay |
| real board | DCD glue、IRQ/callback、endpoint timing 与真实 host enumeration |
| QEMU | kernel/system 配合；不作为 USB device 行为主证据 |

这些证据不能互相替代。

## 示例分组

| 目标 | 示例 |
|---|---|
| CDC / MSC / composite mock | `usb_cdc_mock_smoke`、`usb_msc_mock_smoke`、`usb_msc_cdc_mock_smoke` |
| trace replay / suite | `usb_*_replay_smoke`、`usb_replay_suite_smoke` |
| manifest / board log | `usb_cdc_manifest_smoke`、`usb_msc_boardlog_import_smoke` |
| host harness | `usb_host_harness_smoke` |
| runtime capability | `usb_host_runtime_block_smoke`、`usb_host_runtime_channel_smoke`、`usb_host_runtime_multi_smoke` |

## Host runtime

- `usb.host.core` 将 host adapter 包装为 `device::Bus`。
- `usb.host.runtime` 提供 single/list runtime bus。
- `usb.host.runtime_block/channel` 将 discovered MSC/CDC device 导出到稳定 slot。
- `usb.host.runtime_manager` 编排 bus、registry、binding 与 scan/remove/rediscover/reset。

| Fixture | 局部证明 |
|---|---|
| `usb_host_runtime_block_smoke` | MSC attach 到 `block.usb0`；remove 后旧 slot 返回 `noent` |
| `usb_host_runtime_channel_smoke` | CDC attach 到 `io.usb0`；remove 后 read/write/flush 返回 `noent` |
| `usb_host_runtime_multi_smoke` | MSC/CDC 增量扫描、单设备 remove/rediscover 与 runtime sidecar |

共享 fixture 位于 `Examples/usb/support/`。runtime smoke 通过 `RuntimeManager` 验证增量扫描、detach、
remove、rediscover 与 block/channel export；不手工复制 `device::Registry + BusManager` 装配。

Multi smoke 的 sidecar 保留 `recent_transitions`，摘要反映场景结束状态。由于 fixture 最后执行
`remove + forget + unexport`，`published_capabilities=[]` 以及 publish/export `missing=2` 是预期终态，
不表示导出失败。字段与导出 target 以该目录 CMake/source 为准。

## Runner

```powershell
./scripts/usb_native_smoke.ps1
```

`-ConfigureOnly` 只配置，`-Clean` 清理后重跑。Host 基线使用 clang + Ninja；MCU 工程使用自己的
Arm toolchain。实际 target 与 pass/fail 以脚本当前输出为准。
