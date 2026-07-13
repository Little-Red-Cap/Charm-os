# USB Native Mock/Replay 工作流

## 文档状态

- `status`: `supporting`
- `scope`: USB host-side 回归入口与证据边界
- `runner`: [`usb_native_smoke.ps1`](../../scripts/usb_native_smoke.ps1)

Native mock/replay 是平台无关 USB 代码的快速回归入口，不替代真实 DCD/HCD 或主机兼容性测试。
具体 case 列表由 runner 和 [`Examples/usb/README.md`](../../Examples/usb/README.md) 维护。

## 验证分层

| 层级 | 适合验证 | 不证明 |
|---|---|---|
| native mock/replay | descriptor、EP0、CDC/MSC class、复合装配、trace/boardlog replay | IRQ、DMA、cache、端点时序、真实枚举 |
| real board | DCD/HCD glue、callback/IRQ、端点和 OS host 交互 | 其它平台自动成立 |
| QEMU | kernel/runtime 联动与浅层装配 | USB device correctness 或真实控制器行为 |

UAC 当前没有与 CDC/MSC 同等级的 native runtime smoke，不能从 module 或 descriptor 存在推断覆盖。

## Runner

仓库根目录执行：

```powershell
./scripts/usb_native_smoke.ps1
```

参数：

- `-Jobs <N>`：传给各 case 的 build；
- `-ConfigureOnly`：只 configure，不 build/run；
- `-Clean`：配置每个 case 前删除它原有的 build directory。

Runner 当前为每个 case 创建独立的 `cmake-build-usb-*-clang`，不会复用单一 build tree，也不会在
成功后自动删除。在磁盘受限环境中不要直接运行完整入口；先确认空间，或按需要手工配置单个 case
到统一临时目录。

## 判定

- configure 成功只证明 CMake/toolchain 可生成目标；
- executable exit `0` 只证明对应 fixture 的断言通过；
- replay 通过不证明输入来自真实板，也不证明未覆盖分支；
- boardlog format 与场景范围分别见
  [`usb_boardlog_format.md`](usb_boardlog_format.md) 和
  [`usb_boardlog_coverage_matrix.md`](usb_boardlog_coverage_matrix.md)。
