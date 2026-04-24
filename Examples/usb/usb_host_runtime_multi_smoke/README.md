# usb_host_runtime_multi_smoke

这个样板演示同一条 USB Host runtime bus 同时发现多个设备：

- `usb::host::RuntimeManager`
- `usb::host::MscBlockRuntimeBinding`
- `usb::host::CdcChannelRuntimeBinding`

它验证了下面这条路径：

- 一条 `HostBus` 通过 runtime manager 同时枚举 MSC 和 CDC 两个 discovered device
- 两个 `RuntimeDriver` 分别导出稳定 `block.usb0` 和 `io.usb0`
- 支持单设备 remove 后重新进入枚举
- 增量扫描不会重复重新初始化未变化的设备
- `remove` 后旧 capability 指针不悬挂，都会退回到 `noent`
- 可以导出一份最小 `runtime_observe` sidecar，作为真实 `PublishState / ExportState / recent_transitions` 生产面

## 构建

```powershell
cmake -S Examples/usb/usb_host_runtime_multi_smoke `
  -B cmake-build-usb-host-runtime-multi-smoke `
  -G Ninja
cmake --build cmake-build-usb-host-runtime-multi-smoke
```

## 运行

```powershell
.\cmake-build-usb-host-runtime-multi-smoke\usb-host-runtime-multi-smoke.exe
```

运行成功时会输出：

```text
[OK] usb-host-runtime-multi-smoke passed
```

## 导出 runtime observe sidecar

```powershell
cmake --build cmake-build-usb-host-runtime-multi-smoke --target export_usb_host_runtime_multi_runtime_observe
python ./scripts/validate_materialized_graph_artifacts.py `
  ./cmake-build-usb-host-runtime-multi-smoke/usb_host_runtime_multi.runtime_observe.json
```

当前导出的 sidecar 会保留整条场景的真实 `recent_transitions`，
但摘要字段反映的是场景结束时的最终状态。
因为这个 smoke 会在结尾执行 `remove + forget + unexport`，
所以 `published_capabilities` 为空、`publish_state_summary.missing = 2`、
`export_state_summary.missing = 2` 都是预期结果，不代表导出失败。

也可以手动指定输出路径：

```powershell
.\cmake-build-usb-host-runtime-multi-smoke\usb-host-runtime-multi-smoke.exe `
  --runtime-observe out/runtime_observe.snapshot.json
```
