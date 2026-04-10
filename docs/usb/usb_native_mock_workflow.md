# USB 原生 mock 验证工作流

本文档定义 Charm USB 自研栈当前推荐的验证策略。

## 结论

对当前仓库阶段，`PC 原生 mock backend` 是 USB 自研栈的主验证路径。

原因很直接：

- 平台无关层天然适合在 PC 上快速回归
- 描述符、EP0、类驱动与复合设备装配需要高频、低成本、可脚本化验证
- QEMU 对真实 USB 设备行为的覆盖有限，尤其不适合作为当前问题的主验证环境
- 真机调试仍然重要，但更适合验证板级 DCD glue、回调桥接与实际主机交互

## 推荐验证层次

### 第一层：PC 原生 mock/replay

目标：压住平台无关 USB 行为回归。

建议覆盖：

- 设备描述符与配置描述符
- EP0 标准请求与状态阶段
- CDC/MSC/UAC 等类驱动最小行为
- 复合设备接口布局、IAD 与 endpoint 分配
- trace 回放、manifest 聚合、suite 聚合

当前样例入口位于：

- `Examples/usb/usb_cdc_mock_smoke`
- `Examples/usb/usb_msc_mock_smoke`
- `Examples/usb/usb_msc_cdc_mock_smoke`
- `Examples/usb/usb_replay_suite_smoke`

仓库脚本入口：

- `scripts/usb_native_smoke.ps1`

默认工具链：

- `clang`
- `Ninja`

当前默认脚本会运行：

- `usb-cdc-mock-smoke`
- `usb-msc-mock-smoke`
- `usb-replay-suite-smoke`

### 第二层：板级真机验证

目标：验证真实 DCD/IRQ/端点回调与主机侧枚举行为。

重点关注：

- `board_usb.cppm` 是否保持为纯板级 DCD glue
- 中断回调到 `usb::driver::DcdDeviceAdapter` 的桥接是否正确
- `IN complete` / `OUT data` / `SETUP` 的实机时序是否与 mock 契约一致
- 复合设备在 Windows/Linux/macOS 上的枚举表现

默认工具链：

- `arm-none-eabi`

### 第三层：QEMU 补充验证

目标：补充系统主线验证，而不是替代 USB 真机与原生 mock。

适用场景：

- 内核/调度/bringup 主线烟测
- 非 USB 设备行为回归
- 对 USB 仅做很浅层的联动验证

不建议把 QEMU 作为当前 USB device correctness 的唯一依据。

## 维护建议

- 新增 USB 功能时，优先补 `mock` 或 `replay` 用例，再做板级联调
- 新增板级 workaround 时，尽量把平台无关语义回收进 `Modules/io/usb`
- 保持 `board glue` 与 `device core` 的职责边界清晰
- 优先让 trace、manifest、suite 成为长期回归资产

## 快速命令

在仓库根目录运行：

```powershell
./scripts/usb_native_smoke.ps1
```

如果只想提前生成构建目录：

```powershell
./scripts/usb_native_smoke.ps1 -ConfigureOnly
```
