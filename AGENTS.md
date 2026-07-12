# Charm 仓库 Agent 启动约定

详细规则、技能、模板、工作流与背景材料继续放在 `docs/` 中维护；这里保持简短、稳定、直接可执行。

## 先知道这些

- rg不可用，需用Powershell原生
- 一律使用 UTF-8 编码，包括终端 PowerShell 命令与读写文件，杜绝乱码。
- 使用中文进行沟通。
- 使用中文进行 Git 提交。
- `git commit` 相关操作优先遵守当前会话策略和用户当次指令。
- 如果当前会话明确允许提交，提交信息使用中文。
- 如果改动是临时性的、探索性的、或尚未完成验证，可以先不提交。
- 如果包含多个逻辑独立的改动，优先分批提交，保持历史清晰。
- 当行为、契约、工作流、使用方式发生变化时，按需同步更新相关文档。
- 本环境已配置好 QEMU；在有助于验证运行时行为时，可以按需使用。
- 本仓库本地构建目录统一使用 `cmake-build-*` 命名。
- `cmake-build-*` 已加入 Git 忽略规则，可放心用于临时构建与验证。

## 当前停线规则

Charm 当前处于核心收敛阶段。除非用户明确批准，否则暂停：

- 新增核心概念或 canonical 术语；
- 新增公共 API 或公共 Core 类型；
- 新增顶层目录或架构主线；
- 大规模 CMake 能力扩张或目录迁移。

任何拟进入 canonical 文档或公共 Core 代码的新名词，必须先通过根目录
[`CONSTITUTION.md`](CONSTITUTION.md) 的六问审查并获得明确裁决。现有名词没有祖父条款。

## 权威与文档信任顺序

`AGENTS.md` 只负责 Agent 的操作与协作规则。涉及 Charm 的身份、Core 准入和核心语义时，
默认按下面的优先级判断：

1. 根目录 `CONSTITUTION.md`
2. `docs/architecture/charm_core_contract.md`
3. 根目录 `README.md`、`docs/README.md` 与相关目录的 `README.md`
4. 已标为 supporting 的专题 `*_contract.md` / `*_overview.md`，仅在各自范围内有效
5. 标为 exploration 的 `*_plan.md` / `*_roadmap.md` / `*_draft.md` / `*_review.md` / `*_v0.md`
6. `reference/*` / `generated/*` / `archive/*`

补充：

- 如果子目录下存在更深层的 `AGENTS.md`，则由更深层文件覆盖对应范围内的约定。
- 不要把 `reference/*` 或第三方对照材料误当成当前契约入口。
- supporting 契约可以约束局部实现，但不能反向定义 Charm Core。
- 文件名包含 `contract` 不自动获得 canonical 身份；文档状态和上位裁决优先。

## 当前仓库特别提醒

- Charm 的正式定位和 MVP 以 `CONSTITUTION.md` 与
  `docs/architecture/charm_core_contract.md` 为准。
- `docs/repo_governance.md`、`docs/current_tracks_index.md` 记录停线前的多战线状态，
  当前只作为 supporting snapshot，不是核心身份入口。
- `docs/reference/vsf/*` 主要保留为早期历史参考，不是当前主路线入口。
- `docs/system/minimal_kernel_task_syscall_frame_contract.md` 当前存在历史编码损坏，待恢复，不作为首选入口。
- 如果当前在看 minimal-kernel runtime 总证据链 / 上半层 + 下半层合并验收，优先读：
  - `docs/system/minimal_kernel_runtime_evidence_bundle_contract.md`
- 如果当前在看 minimal-kernel host smoke / 冷启动与热复用证据链，优先读：
  - `docs/system/minimal_kernel_host_smoke_bundle_contract.md`
- 如果当前在看最小 syscall / trap 链，恢复前优先读：
  - `docs/system/minimal_kernel_task_syscall_table_contract.md`
  - `docs/system/minimal_kernel_trap_syscall_contract.md`
  - `docs/system/minimal_kernel_trap_ingress_contract.md`
  - `docs/system/armv7a_runtime_trap_mapping_contract.md`

## 任务直达路由

不要先把 `docs/` 或 `docs/agent/` 整体读一遍。先识别任务，再走最短路径。

### 我在做代码审查

先读：`docs/agent/routes/review.md`

### 我在做代码生成或模块骨架设计

先读：`docs/agent/routes/codegen.md`

### 我在做架构讨论 / 能力归属 / 分层判断

先读：`docs/agent/routes/architecture.md`

### 我在做文档整理 / 路由清理

先读：`docs/agent/routes/docs.md`

### 我在修乱码 / 编码问题

先读：`docs/agent/routes/utf8.md`

### 我在看 init.graph / 装配问题

先读：`docs/agent/routes/init-graph.md`

### 我在看 IO 契约 / Channel / Reactor / Registry

先读：`docs/agent/routes/io.md`

### 我在看 capability map / 能力归属

先读：`docs/agent/routes/capability.md`

### 我在看 block device / 存储接线

先读：`docs/agent/routes/block-device.md`

### 我在看 CMake / preset / 构建接线

先读：`docs/agent/routes/build.md`

## 第二跳入口

如果任务需要更完整的 Agent 体系说明，再进入：

- `docs/agent/routes/README.md`
- `docs/agent/README.md`

前者负责第二跳任务卡片，后者负责 rules / skills / workflows / templates / glossary 的完整关系说明；两者都不是第一触点。
