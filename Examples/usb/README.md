# USB 原生验证入口

本目录存放 Charm USB 自研栈的最小示例与原生验证样例。

当前建议把 USB 验证分成三层：

- `PC 原生 mock/replay`：主验证链，覆盖平台无关层、描述符、EP0、类驱动状态机与复合设备装配
- `板级真机`：次验证链，覆盖 DCD glue、IRQ/回调、端点时序与真实主机枚举
- `QEMU`：补充验证链，适合内核/系统主线，不作为当前 USB 设备行为的主验证环境

## 核心样例

- `usb_cdc_minimal`：最小 CDC 枚举骨架
- `usb_cdc_mock_smoke`：CDC 原生 mock 冒烟
- `usb_host_harness_smoke`：host harness / event-action 序列冒烟
- `usb_host_runtime_block_smoke`：USB Host discovery -> runtime driver -> stable block capability 冒烟
- `usb_host_runtime_channel_smoke`：USB Host discovery -> runtime driver -> stable channel capability 冒烟
- `usb_host_runtime_multi_smoke`：单条 USB Host runtime bus 同时导出 block/channel capability，覆盖 `RuntimeManager`、增量扫描与 `remove -> rediscover`
- `usb_msc_mock_smoke`：MSC 原生 mock 冒烟
- `usb_msc_cdc_mock_smoke`：MSC + CDC 复合设备原生 mock 冒烟
- `usb_cdc_replay_smoke`：CDC trace 回放
- `usb_msc_replay_smoke`：MSC trace 回放
- `usb_msc_cdc_replay_smoke`：复合设备 trace 回放
- `usb_cdc_manifest_smoke`：manifest 组织的 CDC 用例
- `usb_replay_suite_smoke`：suite 级聚合入口
- `usb_msc_boardlog_import_smoke`：板级日志导入与回放验证

## Host runtime 入口

当前 USB Host runtime 的正式 glue 已经收敛到下面这些模块：

- `usb.host.core`：把 host 入口包装成 `device::Bus`
- `usb.host.runtime`：提供 `SingleDeviceRuntimeBus` / `DeviceListRuntimeBus`
- `usb.host.runtime_block`：把 MSC discovered device 导出为稳定 block capability
- `usb.host.runtime_channel`：把 CDC discovered device 导出为稳定 channel capability
- `usb.host.runtime_manager`：把 bus、runtime registry 与多个 binding 编排到一起，提供 `enumerate / scan / remove / rediscover / reset_all`
- `Examples/usb/support/usb_host_runtime_*_support.hpp`：给 host runtime smoke 复用最小后端夹具与 binding harness，避免样板重复拷贝 `MemoryDisk` / `DummyChannel` 和装配样板
- `Examples/usb/support/usb_host_runtime_assert_support.hpp`：收敛 host runtime smoke 共用的最小断言输出，保持样例主体聚焦在场景编排与行为检查

当前三组 host runtime smoke 都统一经由 `RuntimeManager` 编排，
不再手工拼 `device::Registry + BusManager`。

这三组 host runtime smoke 当前共同覆盖：

- 单 discovered device -> stable block export
- 单 discovered device -> stable channel export
- 多 discovered device -> 增量扫描、detach/remove、重新发现与跨 registry 导出

## 推荐用法

仓库根目录下运行：

```powershell
./scripts/usb_native_smoke.ps1
```

它会默认使用 `clang + Ninja` 配置并运行：

- `usb-cdc-mock-smoke`
- `usb-host-harness-smoke`
- `usb-msc-mock-smoke`
- `usb-msc-cdc-mock-smoke`
- `usb-msc-replay-smoke`
- `usb-replay-suite-smoke`
- `usb-msc-boardlog-import-smoke`

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
