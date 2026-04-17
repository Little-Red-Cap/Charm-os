# 文档索引

本页是 `docs/` 的文档地图与路由入口。  
用于按任务或专题查找文档，不替代新同学入门文档 `docs/overview.md`。

如果你是新同学，建议先读 `docs/overview.md`。

## 文档状态说明

为降低认知负担，先区分文档类型：

- `现行入口 / 稳定契约`：默认优先遵守，适合作为日常开发入口。
- `总览 / 路由文档`：帮助理解全局结构与阅读路径，不直接替代具体契约。
- `阶段总结 / 路线图 / v0`：记录某一阶段的结论、快照或方向，不自动等同于长期稳定规范。
- `任务单 / checklist / tasklist`：用于推进、验收、排期，不等同于对外长期接口契约。
- `草案 / draft / 提案`：尚未完全收口，使用时应结合当前代码与上位文档判断。

看到这些标题或后缀时，可先做如下判断：

- `*_contract.md`：优先视为契约文档
- `*_overview.md` / `README.md`：优先视为入口或总览
- `*_tasklist.md` / `*_checklist.md`：优先视为推进材料
- `*_draft.md` / `*草案*.md`：优先视为讨论材料
- `*_v0.md` / `*review*.md` / `*summary*.md`：优先视为阶段成果或阶段记录

## 快速开始（现行 / 稳定入口优先）
- 入门指南：`docs/overview.md`
- 架构总览：`docs/architecture_overview.md`
- 文档总索引：`docs/README.md`
- 板级资料入口：`docs/board/README.md`
- RK3506 上板资料：`docs/board/rk3506/README.md`
- RK3506 post-DDR handoff 契约：`docs/board/rk3506/post_ddr_handoff_contract.md`
- 驱动模型：`docs/architecture/driver_model.md`
- 依赖边界与禁区：`docs/architecture/dependency_contract.md`
- IO 核心契约：`docs/io/io_channel_contract.md`、`docs/io/io_reactor_contract.md`、`docs/io/io_registry_contract.md`
- 装配与启动：`docs/system/init_graph_contract.md`
- ARMv7-A 平台契约：`docs/system/armv7a_platform_contract.md`
- 最小内核运行时 bridge 契约：`docs/system/minimal_kernel_runtime_bridge_contract.md`
- 最小内核 task-side runtime service 契约：`docs/system/minimal_kernel_runtime_service_contract.md`
- 最小内核 task runtime API 契约：`docs/system/minimal_kernel_task_runtime_api_contract.md`
- 最小内核 task syscall API 契约：`docs/system/minimal_kernel_task_syscall_api_contract.md`
- 最小内核 task syscall catalog 契约：`docs/system/minimal_kernel_task_syscall_catalog_contract.md`
- 最小内核 task syscall dispatch 契约：`docs/system/minimal_kernel_task_syscall_dispatch_contract.md`
- 最小内核 task syscall table 契约：`docs/system/minimal_kernel_task_syscall_table_contract.md`
- 最小内核 task syscall frame 契约：`docs/system/minimal_kernel_task_syscall_frame_contract.md`
- 最小内核 trap/syscall 契约：`docs/system/minimal_kernel_trap_syscall_contract.md`
- 最小内核 trap ingress adapter 契约：`docs/system/minimal_kernel_trap_ingress_contract.md`
- ARMv7-A SVC 到 trap frame 映射证据：`docs/system/armv7a_runtime_trap_mapping_contract.md`
- 网络协议栈双表面设计：`docs/io/net_stack_dual_surface_design.md`
- POSIX 兼容总览：`docs/system/posix_support_overview.md`
- POSIX 分层与演进原则：`docs/system/posix_subsystem_principles.md`
- POSIX 三层执行模型：`docs/system/posix_three_layer_contract.md`
- POSIX 用户态运行时：`docs/system/posix_user_runtime_minimal_design.md`
- 输入链路：`docs/input/input_layering_decision.md`

## 阶段文档 / 任务单 / 草案入口

如果你当前做的是路线设计、阶段复盘、排期拆解，优先从这里进入：

- 系统编译器路线图：`docs/architecture/system_compiler_roadmap.md`
- System Compiler 词汇表 v0：`docs/architecture/system_compiler_vocabulary_v0.md`
- Artifact Report v0：`docs/system/artifact_report_v0.md`
- Explain Surface / Artifact Report v0：`docs/system/explain_surface_v0.md`
- 资源契约 v0：`docs/system/resource_contract_v0.md`
- bringup 证据流水线 v0：`docs/system/bringup_evidence_pipeline_v0.md`
- Recipe / Plan 草案：`docs/system/init_plan_recipe_draft.md`
- Init Plan Review 规则：`docs/system/init_plan_review_rules.md`
- Materialized Graph 观察导出：`docs/system/init_materialized_graph_observe.md`
- Materialized Graph 工具链里程碑：`docs/system/init_materialized_graph_tooling_milestone.md`
- 网络 socket v0 契约：`docs/io/net_socket_v0_contract.md`
- 网络协议栈阶段复盘：`docs/io/net_stack_stage_review.md`
- 网络协议栈底座收口任务单：`docs/io/net_stack_foundation_tasklist.md`
- 网络底座 v0 关单清单：`docs/io/net_stack_v0_closure_checklist.md`

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
    B --> M["board/*"]
    B --> J["system/*"]

    F --> K["storage/*"]
    F --> L["usb/*"]
    F --> M
    F --> D
```

## 按任务找文档

| 我要做什么 | 先看什么 |
| --- | --- |
| 看 Charm 中长期主轴 | `docs/architecture/system_compiler_roadmap.md` → `docs/architecture/charm_methodology_charter.md` |
| 看 system compiler 核心词汇与当前仓库映射 | `docs/architecture/system_compiler_vocabulary_v0.md` → `docs/architecture/system_compiler_roadmap.md` |
| 看 system compiler 最小结论对象怎么长 | `docs/system/artifact_report_v0.md` → `docs/system/explain_surface_v0.md` → `schemas/README.md` |
| 看 system compiler 如何对外解释自己 | `docs/system/explain_surface_v0.md` → `docs/system/init_materialized_graph_observe.md` → `schemas/README.md` |
| 看资源边界如何进入系统语言 | `docs/system/resource_contract_v0.md` → `docs/system/ssu_contract.md` → `docs/system/init_graph_contract.md` |
| 看 bringup 如何从装配图变成证据 | `docs/system/bringup_evidence_pipeline_v0.md` → `docs/system/init_materialized_graph_observe.md` → `docs/system/init_graph_contract.md` |
| 新增板级能力 | `docs/system/init_graph_contract.md` → `docs/io/io_layering_overview.md` |
| 上 RK3506 板 / 查早期寄存器 | `docs/board/rk3506/README.md` → `docs/board/rk3506/post_ddr_handoff_contract.md` → `docs/system/armv7a_platform_contract.md` |
| 推进 ARMv7-A 平台 bring-up | `docs/system/armv7a_platform_contract.md` → `docs/boot/bootloader_overview.md` |
| 推进最小内核运行时 glue / bridge | `docs/system/minimal_kernel_runtime_bridge_contract.md` → `docs/system/armv7a_minimal_kernel_staging_plan.md` |
| 推进 task-side runtime service / syscall facade | `docs/system/minimal_kernel_runtime_service_contract.md` → `docs/system/minimal_kernel_trap_syscall_contract.md` |
| 推进 task runtime API / future current-task syscall facade | `docs/system/minimal_kernel_task_runtime_api_contract.md` → `docs/system/minimal_kernel_runtime_service_contract.md` |
| 推进 task syscall API / future syscall surface naming | `docs/system/minimal_kernel_task_syscall_api_contract.md` → `docs/system/minimal_kernel_task_runtime_api_contract.md` |
| 推进最小 syscall 编号 / catalog / trap 映射 | `docs/system/minimal_kernel_task_syscall_catalog_contract.md` → `docs/system/minimal_kernel_task_syscall_api_contract.md` |
| 推进最小 syscall request / dispatch bridge | `docs/system/minimal_kernel_task_syscall_dispatch_contract.md` → `docs/system/minimal_kernel_task_syscall_catalog_contract.md` |
| 推进最小静态 syscall handler table | `docs/system/minimal_kernel_task_syscall_table_contract.md` → `docs/system/minimal_kernel_task_syscall_dispatch_contract.md` |
| 推进最小 numbered syscall frame bridge | `docs/system/minimal_kernel_task_syscall_frame_contract.md` → `docs/system/minimal_kernel_task_syscall_table_contract.md` |
| 推进最小 trap / syscall 边界 | `docs/system/minimal_kernel_trap_syscall_contract.md` → `docs/system/armv7a_minimal_kernel_staging_plan.md` |
| 推进 trap frame / ingress adapter 边界 | `docs/system/minimal_kernel_trap_ingress_contract.md` → `docs/system/minimal_kernel_trap_syscall_contract.md` |
| 推进 ARMv7-A SVC / trap frame 映射验证 | `docs/system/armv7a_runtime_trap_mapping_contract.md` → `docs/system/minimal_kernel_trap_ingress_contract.md` |
| 设计驱动/外设模型 | `docs/architecture/driver_model.md` → `docs/architecture/device_model_overview.md` → `docs/io/io_layering_overview.md` |
| 接入文件系统 | `docs/storage/block_device_contract.md` → `docs/storage/fs_vfs_mount_rules.md` |
| 实现 USB 设备 | `docs/usb/usb_arch_plan.md` → `docs/usb/usb_dsl_overview.md` |
| 实现 USB Host 运行期发现 | `docs/architecture/driver_model.md` → `docs/architecture/device_model_overview.md` |
| 开始改代码 | `docs/overview.md` → `docs/project/standards/项目C++编码要求.md` |
| 和 AI 协作 | `docs/agent/README.md` |
| 做代码审查 | `docs/agent/skills/code-review/` |
| 做网络协议栈设计 | `docs/io/net_stack_dual_surface_design.md` |
| 看网络协议栈阶段复盘 | `docs/io/net_stack_stage_review.md` |
| 拆网络底座收口任务 | `docs/io/net_stack_foundation_tasklist.md` |
| 看网络底座是否已可关单 | `docs/io/net_stack_v0_closure_checklist.md` |
| 看 Vivid object-level widget `observe_*` 与 SoA `SceneAccess` 边界 | `docs/ui/vivid_widget_state_observe.md` → `Examples/ui/vivid/widget_state_demo` → `Examples/ui/vivid/scene_state_demo` |
| 做主框架全仓体检 / 收敛排期 | `docs/project/tracking/主框架全仓审查与收敛_backlog.md` |
| POSIX 兼容总览 | `docs/system/posix_support_overview.md` |
| POSIX 分层与演进原则 | `docs/system/posix_subsystem_principles.md` |
| POSIX 三层执行模型 | `docs/system/posix_three_layer_contract.md` |
| POSIX 用户态运行时 | `docs/system/posix_user_runtime_minimal_design.md` |
| POSIX 兼容/BusyBox 验收 | `docs/system/posix_compat_roadmap.md` |
| Linux 生态兼容任务清单 | `docs/system/posix_linux_compat_tasklist.md` |
| POSIX spawn 草案 | `docs/system/posix_spawn_minimal_design.md` |
| POSIX fd 表草案 | `docs/system/posix_fd_table_minimal_design.md` |
| POSIX errno 映射 | `docs/system/posix_errno_mapping.md` |
| POSIX 错误语义约定 | `docs/system/posix_error_semantics.md` |
| BusyBox 验收清单 | `docs/system/posix_busybox_phase_checklist.md` |
| POSIX v0 收口清单 | `docs/system/posix_v0_closure_checklist.md` |

## 按专题索引

### 文档分层图
```text
docs/
├─ overview.md                新同学 10 分钟入口
├─ architecture_overview.md   全局架构图
├─ README.md                  文档总索引
├─ agent/                     AI / Agent 协作体系
├─ architecture/              架构规则与依赖边界
├─ board/                     板级 bring-up 与 SoC 资料收口
├─ io/                        IO 核心契约
├─ system/                    装配、启动、系统服务
└─ ...
```

### 总览
- `docs/architecture_overview.md`

### 架构与依赖
- `docs/architecture/system_compiler_roadmap.md`
- `docs/architecture/system_compiler_vocabulary_v0.md`
- `docs/architecture/dependency_contract.md`
- `docs/architecture/dependency_whitelist.md`
- `docs/architecture/driver_model.md`
- `docs/architecture/device_model_overview.md`
- `docs/architecture/signal_state_contract_v0.md`
- `docs/architecture/signal_state_v0.md`
- `docs/architecture/capability_recovery_rules.md`
- `docs/architecture/capability_recovery_matrix.md`

### Board / 板级 bring-up
- `docs/board/README.md`
- `docs/board/rk3506/README.md`
- `docs/board/rk3506/post_ddr_handoff_contract.md`

### IO 与输入
- `docs/io/io_layering_overview.md`
- `docs/io/io_channel_contract.md`
- `docs/io/io_reactor_contract.md`
- `docs/io/io_registry_contract.md`
- `docs/io/net_stack_dual_surface_design.md`
- `docs/io/net_socket_v0_contract.md`
- `docs/io/net_stack_stage_review.md`
- `docs/io/net_stack_foundation_tasklist.md`
- `docs/io/net_stack_v0_closure_checklist.md`
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
- `docs/architecture/driver_model.md`（USB Host runtime / capability export）
- `docs/architecture/device_model_overview.md`（USB Host discovery / lifecycle）

### 系统与启动
- `docs/system/artifact_report_v0.md`
- `docs/system/explain_surface_v0.md`
- `docs/system/resource_contract_v0.md`
- `docs/system/bringup_evidence_pipeline_v0.md`
- `docs/system/init_graph_contract.md`
- `docs/system/init_plan_recipe_draft.md`
- `docs/system/init_plan_review_rules.md`
- `docs/system/init_materialized_graph_observe.md`
- `docs/system/init_materialized_graph_tooling_milestone.md`
- `docs/system/armv7a_platform_contract.md`
- `docs/system/minimal_kernel_runtime_bridge_contract.md`
- `docs/system/minimal_kernel_runtime_service_contract.md`
- `docs/system/minimal_kernel_task_runtime_api_contract.md`
- `docs/system/minimal_kernel_task_syscall_api_contract.md`
- `docs/system/minimal_kernel_task_syscall_catalog_contract.md`
- `docs/system/minimal_kernel_task_syscall_dispatch_contract.md`
- `docs/system/minimal_kernel_task_syscall_table_contract.md`
- `docs/system/minimal_kernel_task_syscall_frame_contract.md`
- `docs/system/minimal_kernel_trap_syscall_contract.md`
- `docs/system/minimal_kernel_trap_ingress_contract.md`
- `docs/system/armv7a_runtime_trap_mapping_contract.md`
- `docs/system/service_component_init.md`
- `docs/system/power_lowpower_overview.md`
- `docs/system/at_system.md`
- `docs/system/av_pipeline_overview.md`
- `docs/system/posix_support_overview.md`
- `docs/system/posix_subsystem_principles.md`
- `docs/system/posix_three_layer_contract.md`
- `docs/system/posix_user_runtime_minimal_design.md`
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
- `docs/project/escape_hatches.md`
- `docs/project/tracking/主框架全仓审查与收敛_backlog.md`
- `docs/project/tracking/推进TODO与分工.md`
- `docs/project/tracking/refactor_todo_ownership.md`
- `docs/project/tracking/player_issue_log.md`
- `docs/project/tooling/Powershell设置utf8.md`

### UI
- `docs/ui/player_ui.md`
- `docs/ui/player_vivid_patterns.md`
- `docs/ui/vivid_widget_state_observe.md`

### 方法论总纲
- `docs/architecture/charm_methodology_charter.md`
