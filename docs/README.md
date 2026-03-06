# 文档索引

本目录按主题分组。第一次阅读建议按“入门路径”，再按需深入各专题。

## 入门路径（第一次看这里）
- 架构总览：`docs/architecture_overview.md`
- 依赖边界与禁区：`docs/architecture/dependency_contract.md`
- IO 核心契约：`docs/io/io_channel_contract.md`、`docs/io/io_reactor_contract.md`、`docs/io/io_registry_contract.md`
- 装配与启动：`docs/system/init_graph_contract.md`
- 输入链路：`docs/input/input_layering_decision.md`

## 常见问题入口
- 我要新增一个板级能力（UART/SPI/Flash）：从 `docs/system/init_graph_contract.md` 开始，再看 `docs/io/io_layering_overview.md`
- 我要接入文件系统：先读 `docs/storage/block_device_contract.md` 与 `docs/storage/fs_vfs_mount_rules.md`
- 我要做 USB 设备：先读 `docs/usb/usb_arch_plan.md` 与 `docs/usb/usb_dsl_overview.md`
- 我要理解输入链路：`docs/input/input_layering_decision.md` 与 `docs/input/input_protocol_map.md`

## 总览
- `docs/architecture_overview.md`

## 架构与依赖
- `docs/architecture/dependency_contract.md`
- `docs/architecture/dependency_whitelist.md`
- `docs/architecture/device_model_overview.md`
- `docs/architecture/capability_recovery_rules.md`
- `docs/architecture/capability_recovery_matrix.md`

## IO 与输入
- `docs/io/io_layering_overview.md`
- `docs/io/io_channel_contract.md`
- `docs/io/io_reactor_contract.md`
- `docs/io/io_registry_contract.md`
- `docs/input/input_layering_decision.md`
- `docs/input/input_protocol_map.md`

## 存储/文件系统
- `docs/storage/block_device_contract.md`
- `docs/storage/mal_overview.md`
- `docs/storage/mal_fatfs_demo.md`
- `docs/storage/fs_vfs_mount_rules.md`
- `docs/storage/fs_block_cache_strategy.md`
- `docs/storage/fs_fatfs_demo.md`
- `docs/storage/filex_charm_map.md`

## USB
- `docs/usb/usb_arch_plan.md`
- `docs/usb/usb_dsl_overview.md`
- `docs/usb/usb_cdc_contract.md`
- `docs/usb/usb_strings_overview.md`

## 系统与启动
- `docs/system/init_graph_contract.md`
- `docs/system/service_component_init.md`
- `docs/system/power_lowpower_overview.md`
- `docs/system/at_system.md`
- `docs/system/av_pipeline_overview.md`
- `docs/boot/bootloader_overview.md`
- `docs/boot/bootloader_xymodem.md`

## Trace
- `docs/trace/trace_core_entry.md`
- `docs/trace/trace_core_ids.md`

## VSF 参考
- `docs/vsf/vsf_comparison.md`
- `docs/vsf/vsf_component_scan.md`
- `docs/vsf/vsf_storage_map.md`
- `docs/vsf/vsf_tcpip_map.md`
- `docs/vsf/vsf_usb_map.md`

## 项目规范与协作
- `docs/project/project_conventions.md`
- `docs/project/项目C++编码要求.md`
- `docs/project/《协作期待与规范》.md`
- `docs/project/《现代 C++ 单片机代码协作认知》.md`
- `docs/project/推进TODO与分工.md`
- `docs/project/refactor_todo_ownership.md`
- `docs/project/player_issue_log.md`
- `docs/project/Powershell设置utf8.md`

## UI
- `docs/ui/player_ui.md`
