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
