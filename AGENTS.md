# Charm 仓库 Agent 启动约定

这份文件是 Agent 进入仓库后的第一跳启动页。

目标只有一个：把“必须先知道的共识”放在仓库根目录，避免 Agent 先经过多层 README 再拿到真正关键的协作约定。

详细规则、技能、模板、工作流与背景材料继续放在 `docs/` 中维护；这里保持简短、稳定、直接可执行。

## 先知道这些

- `git commit` 相关操作优先遵守当前会话策略和用户当次指令。
- 如果当前会话明确允许提交，提交信息使用中文。
- 如果改动是临时性的、探索性的、或尚未完成验证，可以先不提交。
- 如果包含多个逻辑独立的改动，优先分批提交，保持历史清晰。
- 当行为、契约、工作流、使用方式发生变化时，按需同步更新相关文档。
- 本环境已配置好 QEMU；在有助于验证运行时行为时，可以按需使用。
- 本仓库本地构建目录统一使用 `cmake-build-*` 命名。
- `cmake-build-*` 已加入 Git 忽略规则，可放心用于临时构建与验证。

## 文档信任顺序

在 Charm 仓库里，默认按下面的优先级判断“什么更可信”：

1. 根目录 `AGENTS.md`
2. 相关目录的 `README.md` 与 `docs/README.md`
3. `*_contract.md` / `*_overview.md`
4. `*_plan.md` / `*_roadmap.md` / `*_draft.md` / `*_review.md` / `*_v0.md`
5. `reference/*` / `generated/*`

补充：

- 如果子目录下存在更深层的 `AGENTS.md`，则由更深层文件覆盖对应范围内的约定。
- 不要把 `reference/*` 或第三方对照材料误当成当前契约入口。

## 当前仓库特别提醒

- `docs/reference/vsf/*` 主要保留为早期历史参考，不是当前主路线入口。
- `docs/system/minimal_kernel_task_syscall_frame_contract.md` 当前存在历史编码损坏，待恢复，不作为首选入口。
- 如果当前在看最小 syscall / trap 链，恢复前优先读：
  - `docs/system/minimal_kernel_task_syscall_table_contract.md`
  - `docs/system/minimal_kernel_trap_syscall_contract.md`
  - `docs/system/minimal_kernel_trap_ingress_contract.md`
  - `docs/system/armv7a_runtime_trap_mapping_contract.md`

## 任务直达路由

不要先把 `docs/` 或 `docs/agent/` 整体读一遍。先识别任务，再走最短路径。

### 我在做代码审查

先读：

- `docs/agent/rules/charm-architecture.md`
- `docs/agent/rules/embedded-modern-cpp.md`
- `docs/architecture/signal_state_contract_v0.md`（涉及事件连接时）
- `docs/agent/skills/code-review/SKILL.md`

### 我在做代码生成或模块骨架设计

先读：

- `docs/agent/rules/collaboration.md`
- `docs/agent/rules/embedded-modern-cpp.md`
- `docs/agent/rules/charm-architecture.md`
- `docs/architecture/signal_state_contract_v0.md`（涉及事件连接时）
- `docs/agent/skills/codegen/SKILL.md`

### 我在做架构讨论 / 能力归属 / 分层判断

先读：

- `docs/agent/rules/charm-architecture.md`
- `docs/agent/rules/embedded-modern-cpp.md`
- `docs/architecture/signal_state_contract_v0.md`（涉及同域通知 / 状态 / post 时）
- `docs/agent/glossary.md`
- `docs/agent/skills/architect-review/SKILL.md`

### 我在做文档整理 / 路由清理

先读：

- `docs/documentation_maintenance.md`
- `docs/README.md`
- 目标目录下的 `README.md`
- `docs/agent/skills/charm-docs-minimal/SKILL.md`

### 我在修乱码 / 编码问题

先读：

- `docs/documentation_maintenance.md`
- `docs/agent/skills/charm-docs-utf8/SKILL.md`

### 我在看 init.graph / 装配问题

先读：

- `docs/system/init_graph_contract.md`
- `docs/agent/skills/charm-init-graph/SKILL.md`

### 我在看 IO 契约 / Channel / Reactor / Registry

先读：

- `docs/io/io_channel_contract.md`
- `docs/io/io_reactor_contract.md`
- `docs/io/io_registry_contract.md`
- `docs/agent/skills/charm-io-contracts/SKILL.md`

### 我在看 capability map / 能力归属

先读：

- `docs/capability_map.md`
- `docs/agent/skills/charm-capability-map/SKILL.md`

### 我在看 block device / 存储接线

先读：

- `docs/storage/README.md`
- `docs/agent/skills/charm-block-device/SKILL.md`

### 我在看 CMake / preset / 构建接线

先读：

- `CMakePresets.json`
- `docs/project/README.md`
- `docs/agent/skills/charm-cmake/SKILL.md`

## 第二跳入口

如果任务需要更完整的 Agent 体系说明，再进入：

- `docs/agent/README.md`

它负责 rules / skills / workflows / templates / glossary 的完整关系说明，但不是第一触点。
