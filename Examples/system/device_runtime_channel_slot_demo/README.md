# device_runtime_channel_slot_demo

这个样板演示 runtime-discovered channel 如何通过
`io::ChannelSlotExport + device::make_runtime_driver<ContextT>(...)`
收口到稳定 capability。

它验证了下面这条闭环：

- `RuntimeBus` 在运行期枚举一个 `usb.cdc` 风格设备
- `RuntimeDriver` 在 `init` 时把后端 channel attach 到 `io::ChannelSlotExport`
- `io.registry` 对外始终只暴露稳定 capability `io.usb0`
- `remove` 后旧指针不悬挂，后续 `read/write/flush` 返回 `Errc::noent`

## 构建

```powershell
cmake -S Examples/system/device_runtime_channel_slot_demo `
  -B cmake-build-device-runtime-channel-slot-demo `
  -G Ninja
cmake --build cmake-build-device-runtime-channel-slot-demo
```

## 运行

```powershell
.\cmake-build-device-runtime-channel-slot-demo\device-runtime-channel-slot-demo.exe
```

运行成功时会输出：

```text
[OK] runtime channel slot demo passed
```
