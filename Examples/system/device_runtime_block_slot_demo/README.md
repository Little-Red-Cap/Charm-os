# device_runtime_block_slot_demo

这个样板演示 `docs/architecture/driver_model.md` 中推荐的
“稳定槽位 + 内部存活位”导出路线。

它验证了下面这条闭环：

- `RuntimeBus` 在运行期枚举一个 `usb.msc` 风格设备
- `RuntimeDriver` 在 `init` 时把后端 block device attach 到 `block::DeviceSlotExport`
- `block.registry` 对外始终只暴露稳定 capability `block.usb0`
- `remove` 后旧指针不悬挂，后续访问返回 `Errc::noent`

## 构建

```powershell
cmake -S Examples/system/device_runtime_block_slot_demo `
  -B cmake-build-device-runtime-block-slot-demo `
  -G Ninja
cmake --build cmake-build-device-runtime-block-slot-demo
```

## 运行

```powershell
.\cmake-build-device-runtime-block-slot-demo\device-runtime-block-slot-demo.exe
```

运行成功时会输出：

```text
[OK] runtime block slot demo passed
```
