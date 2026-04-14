# usb_host_runtime_block_smoke

这个样板演示真正位于 `Modules/io/usb/host/*` 下的 host runtime glue：

- `usb::host::SingleDeviceRuntimeBus`
- `usb::host::MscBlockRuntimeBinding`

它验证了下面这条路径：

- USB Host discovery 通过 `HostBus` 把 MSC 设备送入 `device::Registry`
- `RuntimeDriver` 在 `init` 阶段把后端 block device attach 到稳定槽位
- `block.registry` 始终只暴露稳定 capability `block.usb0`
- `remove` 后旧指针不悬挂，访问返回 `Errc::noent`

## 构建

```powershell
cmake -S Examples/usb/usb_host_runtime_block_smoke `
  -B cmake-build-usb-host-runtime-block-smoke `
  -G Ninja
cmake --build cmake-build-usb-host-runtime-block-smoke
```

## 运行

```powershell
.\cmake-build-usb-host-runtime-block-smoke\usb-host-runtime-block-smoke.exe
```

运行成功时会输出：

```text
[OK] usb-host-runtime-block-smoke passed
```
