# usb_host_runtime_channel_smoke

这个样板演示 USB Host CDC runtime glue 如何导出稳定 channel capability：

- `usb::host::SingleDeviceRuntimeBus`
- `usb::host::CdcChannelRuntimeBinding`

它验证了下面这条路径：

- USB Host discovery 通过 `HostBus` 把 CDC 设备送入 `device::Registry`
- `RuntimeDriver` 在 `init` 阶段把后端 channel attach 到稳定槽位
- `io.registry` 始终只暴露稳定 capability `io.usb0`
- `remove` 后旧指针不悬挂，后续 `read/write/flush` 返回 `Errc::noent`

## 构建

```powershell
cmake -S Examples/usb/usb_host_runtime_channel_smoke `
  -B cmake-build-usb-host-runtime-channel-smoke `
  -G Ninja
cmake --build cmake-build-usb-host-runtime-channel-smoke
```

## 运行

```powershell
.\cmake-build-usb-host-runtime-channel-smoke\usb-host-runtime-channel-smoke.exe
```

运行成功时会输出：

```text
[OK] usb-host-runtime-channel-smoke passed
```
