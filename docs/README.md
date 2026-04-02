# 文档索引

本页是 `docs/` 的文档地图与路由入口。  
用于按任务或专题查找文档，不替代新同学入门文档 `docs/overview.md`。

如果你是新同学，建议先读 `docs/overview.md`。

## 快速开始
- 入门指南：`docs/overview.md`
- 架构总览：`docs/architecture_overview.md`
- 依赖边界与禁区：`docs/architecture/dependency_contract.md`
- IO 核心契约：`docs/io/io_channel_contract.md`、`docs/io/io_reactor_contract.md`、`docs/io/io_registry_contract.md`
- 装配与启动：`docs/system/init_graph_contract.md`
- 输入链路：`docs/input/input_layering_decision.md`

## 文档体系图

```mermaid
flowchart TD
    A["docs/overview.md<br/>10 分钟入门"] --> B["docs/architecture_overview.md<br/>全局架构图"]
    A --> C["docs/project/standards/项目C++编码要求.md<br/>开始改代码前必读"]
    A --> D["docs/agent/README.md<br/>AI / Agent 协作入口"]

    E["docs/README.md<br/>文档地图 / 路由入口"] --> F["按任务找文档"]
    E --> G["按专题索引"]

    B --> H["architecture/*"]
    B --> I["io/*"]
    B --> J["system/*"]

    F --> K["storage/*"]
    F --> L["usb/*"]
    F --> D
```

## 按任务找文档

| 我要做什么 | 先看什么 |
| --- | --- |
| 新增板级能力 | `docs/system/init_graph_contract.md` → `docs/io/io_layering_overview.md` |
| 接入文件系统 | `docs/storage/block_device_contract.md` → `docs/storage/fs_vfs_mount_rules.md` |
| 实现 USB 设备 | `docs/usb/usb_arch_plan.md` → `docs/usb/usb_dsl_overview.md` |
| 开始改代码 | `docs/overview.md` → `docs/project/standards/项目C++编码要求.md` |
| 和 AI 协作 | `docs/agent/README.md` |
| 做代码审查 | `docs/agent/skills/code-review/` |
| POSIX 兼容/BusyBox 验收 | `docs/system/posix_compat_roadmap.md` |
| Linux 生态兼容任务清单 | `docs/system/posix_linux_compat_tasklist.md` |
| POSIX spawn 草案 | `docs/system/posix_spawn_minimal_design.md` |
| POSIX fd 表草案 | `docs/system/posix_fd_table_minimal_design.md` |
| POSIX errno 映射 | `docs/system/posix_errno_mapping.md` |
| POSIX 错误语义约定 | `docs/system/posix_error_semantics.md` |
| BusyBox 验收清单 | `docs/system/posix_busybox_phase_checklist.md` |

## 按专题索引

### 文档分层图
```text
docs/
├─ overview.md                新同学 10 分钟入口
├─ architecture_overview.md   全局架构图
├─ README.md                  文档总索引
├─ agent/                     AI / Agent 协作体系
├─ architecture/              架构规则与依赖边界
├─ io/                        IO 核心契约
├─ system/                    装配、启动、系统服务
└─ ...
```

### 总览
- `docs/architecture_overview.md`

### 架构与依赖
- `docs/architecture/dependency_contract.md`
- `docs/architecture/dependency_whitelist.md`
- `docs/architecture/device_model_overview.md`
- `docs/architecture/capability_recovery_rules.md`
- `docs/architecture/capability_recovery_matrix.md`

### IO 与输入
- `docs/io/io_layering_overview.md`
- `docs/io/io_channel_contract.md`
- `docs/io/io_reactor_contract.md`
- `docs/io/io_registry_contract.md`
- `docs/input/input_layering_decision.md`
- `docs/input/input_protocol_map.md`

### 存储/文件系统
- `docs/storage/block_device_contract.md`
- `docs/storage/mal_overview.md`
- `docs/storage/mal_fatfs_demo.md`
- `docs/storage/fs_vfs_mount_rules.md`
- `docs/storage/fs_block_cache_strategy.md`
- `docs/storage/fs_fatfs_demo.md`
- `docs/storage/filex_charm_map.md`

### USB
- `docs/usb/usb_arch_plan.md`
- `docs/usb/usb_dsl_overview.md`
- `docs/usb/usb_cdc_contract.md`
- `docs/usb/usb_strings_overview.md`

### 系统与启动
- `docs/system/init_graph_contract.md`
- `docs/system/service_component_init.md`
- `docs/system/power_lowpower_overview.md`
- `docs/system/at_system.md`
- `docs/system/av_pipeline_overview.md`
- `docs/system/posix_compat_roadmap.md`
- `docs/system/posix_linux_compat_tasklist.md`
- `docs/system/posix_spawn_minimal_design.md`
- `docs/system/posix_fd_table_minimal_design.md`
- `docs/system/posix_errno_mapping.md`
- `docs/system/posix_error_semantics.md`
- `docs/system/posix_busybox_phase_checklist.md`
- `docs/boot/bootloader_overview.md`
- `docs/boot/bootloader_xymodem.md`

### Trace
- `docs/trace/trace_core_entry.md`
- `docs/trace/trace_core_ids.md`

### VSF 参考
- `docs/reference/vsf/vsf_comparison.md`
- `docs/reference/vsf/vsf_component_scan.md`
- `docs/reference/vsf/vsf_storage_map.md`
- `docs/reference/vsf/vsf_tcpip_map.md`
- `docs/reference/vsf/vsf_usb_map.md`

### 项目规范与协作
- `docs/project/standards/project_conventions.md`
- `docs/project/standards/项目C++编码要求.md`
- `docs/project/collaboration/《协作期待与规范》.md`
- `docs/project/collaboration/《现代 C++ 单片机代码协作认知》.md`
- `docs/project/tracking/推进TODO与分工.md`
- `docs/project/tracking/refactor_todo_ownership.md`
- `docs/project/tracking/player_issue_log.md`
- `docs/project/tooling/Powershell设置utf8.md`

### UI
- `docs/ui/player_ui.md`
- `docs/ui/player_vivid_patterns.md`
