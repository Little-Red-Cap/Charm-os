# USB 原生验证入口

本目录存放 Charm USB 自研栈的最小示例与原生验证样例。

当前建议把 USB 验证分成三层：

- `PC 原生 mock/replay`：主验证链，覆盖平台无关层、描述符、EP0、类驱动状态机与复合设备装配
- `板级真机`：次验证链，覆盖 DCD glue、IRQ/回调、端点时序与真实主机枚举
- `QEMU`：补充验证链，适合内核/系统主线，不作为当前 USB 设备行为的主验证环境

## 核心样例

- `usb_cdc_minimal`：最小 CDC 枚举骨架
- `usb_cdc_mock_smoke`：CDC 原生 mock 冒烟
- `usb_msc_mock_smoke`：MSC 原生 mock 冒烟
- `usb_msc_cdc_mock_smoke`：MSC + CDC 复合设备原生 mock 冒烟
- `usb_cdc_replay_smoke`：CDC trace 回放
- `usb_msc_replay_smoke`：MSC trace 回放
- `usb_msc_cdc_replay_smoke`：复合设备 trace 回放
- `usb_cdc_manifest_smoke`：manifest 组织的 CDC 用例
- `usb_replay_suite_smoke`：suite 级聚合入口
- `usb_msc_boardlog_import_smoke`：板级日志导入与回放验证

## 推荐用法

仓库根目录下运行：

```powershell
./scripts/usb_native_smoke.ps1
```

它会默认使用 `clang + Ninja` 配置并运行：

- `usb-cdc-mock-smoke`
- `usb-replay-suite-smoke`

只做配置：

```powershell
./scripts/usb_native_smoke.ps1 -ConfigureOnly
```

清理后重跑：

```powershell
./scripts/usb_native_smoke.ps1 -Clean
```

## 工具链约定

- PC 原生验证优先使用 `clang`
- MCU/板级工程继续使用 `arm-none-eabi`
- `MSVC` 不是当前 USB 主验证基线
