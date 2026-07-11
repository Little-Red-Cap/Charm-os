# SSU 主线状态（阶段总结）

## 当前结论

SSU（可调度语义单元）已经从概念提案进入 Charm 的主线事实层。
它不再只是文档中的架构方向，而是已经具备：

- 契约文档
- 纪律文档
- 内核最小类型骨架
- registry 识别能力
- scheduler 结构化观测接入
- 局部严格模式样板

这意味着 SSU 已越过“脑洞阶段”，开始成为 Charm 的真实系统语义脊柱之一。

## 问题背景

Charm 在进入这一阶段之前，已经同时存在多套执行微体系：

- EDA task / event dispatch
- reactor + pump
- run loop / phase step
- pipeline tick / pull
- audio data plane / DMA demand

入口约束只能限制 import 路径，不能统一这些执行语义。
真正的问题不是“上层没有走入口”，而是“系统需要的执行语义没有被收敛成最短路径”。

SSU 的提出，就是为了解决这个问题：
不统一 API 风格，先统一执行语义。

## 已完成成果

### 1. 契约层

已新增：

- `docs/system/ssu_contract.md`

已明确 SSU 的最小五元语义：

- `ExecutionDomain`
- `TriggerKind`
- `BudgetKind`
- `BlockingKind`
- `Meta`

并明确了 SSU 的立场：

- 统一的是执行语义，不是 Event 类型
- 统一的是可调度单元，不是数据流对象
- 第一阶段不强推大而全的 `Node<In,Out,Policy>`

### 2. 纪律层

已新增：

- `docs/system/ssu_discipline.md`

已建立的阶段 1 纪律：

- 进入 scheduler 的 task，应声明 `ssu_meta()`
- 默认先温和收口，不立刻全仓报错
- 内核提供严格模式开关，可升级为真实编译约束

### 3. 类型层

已新增：

- `Modules/system/kernel/ssu.cppm`

当前内容保持“工具链保守模式”：

- 不使用模块分区
- 少用复杂 STL 组合
- 只保留最薄骨架

已包含：

- `kernel::ssu::Meta`
- `kernel::ssu::SsuUnit`
- `kernel::ssu::eda_adapter`
- `kernel::ssu::as_event_unit(...)`

### 4. Registry / 内核桥接层

已修改：

- `Modules/system/kernel/eda.cppm`

已建立：

- `EdaTaskWithSsu`
- `require_ssu_meta()`
- `validate_task_ssu<Task>()`
- `task_ssu_name(TaskId)`

这意味着 `TaskRegistry` 现在已经具备两项关键能力：

1. 识别 task 是否声明了 `ssu_meta()`
2. 在严格模式下，对缺失 SSU 的 task 做编译期约束

### 5. 观测层

已修改：

- `Modules/system/kernel/scheduler.cppm`
- `Modules/system/kernel/scheduler_export.cppm`

当前 scheduler 已能提供结构化 SSU 观测到：

- task snapshot

`kernel.scheduler_export` 负责把这些结构化观测呈现为：

- `tasks.json`
- `trace.json`
- `trace.csv`
- `ssu_overview.json`（通过 `kernel::format_ssu_overview_json(scheduler, ...)` 输出 trigger/budget/blocking/domain 分布）
- `ssu_hotspots.json`（通过 `kernel::format_ssu_hotspots_json(scheduler, ...)` 输出 dominant submit、demand 占比与风险等级）

这一步非常关键：
SSU 已经进入 observability，而不是只停留在类型声明层。

示例（来自 `Examples/kernel/windows/main_m3.cpp`）：

```json
{"tasks":2,"unnamed":0,"trigger":{"event":2,"io_ready":0,"timer":0,"frame":0,"demand":0},"budget":{"single_step":2,"budgeted":0},"blocking":{"non_blocking":2,"may_block":0},"domain":{"isr_only":0,"task_only":2,"anywhere":0}}
```

```json
{"dominant_submit":"post","submit":{"post":4,"io_ready":0,"demand":0,"timer":0,"replay":0},"submit_total":4,"demand_share_permille":0,"unnamed_count":0,"risk_level":"error","risk":{"unnamed_tasks":0,"no_demand_submit":1,"low_demand_share_warn":1,"low_demand_share_error":1},"unnamed_tasks":[]}
```

### 6. 真实 task 接入

当前已经显式声明 `ssu_meta()` 的真实 task 包括：

- `system.reactor_pump`
  - `task_only + io_ready + budgeted + non_blocking`
- `input.pump`
  - `task_only + timer + budgeted + non_blocking`
- `canopen.pump`
  - `task_only + timer + single_step + non_blocking`

这证明 SSU 已经可以覆盖多种实际执行形态，而不是只能描述单一事件系统。

## 严格模式验证结果

### 样板一：CM7 USB 自研 MSC 主线

目标：

- `Examples/project/player/stn32h747_HQZY/CM7`

已开启：

- `CHARM_KERNEL_REQUIRE_SSU_META=1`
- 可执行目标与 `Charm-os` 库构建都启用严格模式

结果：

- 构建成功

意义：

- SSU 规则已不只是建议
- 至少在一个真实主线 target 上，它已经是编译事实

### 样板二：`Charm-dap`（原 `project/daplink`）

目标：

- 迁移前为 `Examples/project/daplink`，现为独立仓库 `Charm-dap`

操作：

- 为可执行目标与 `Charm-os` 同时开启 `CHARM_KERNEL_REQUIRE_SSU_META=1`

结果：

- 构建成功（ARM/GCC）

意义：

- 第二个干净样板成立
- SSU 严格模式具备重复施工能力，不再是单点样板

### 样板三：`kernel/rtos/qemu`

目标：

- `Examples/kernel/rtos/qemu`

操作：

- 为可执行目标与 `Charm-os` 同时开启 `CHARM_KERNEL_REQUIRE_SSU_META=1`

结果：

- 构建成功（ARM/GCC）

意义：

- 第三个干净样板成立
- SSU 严格模式完成跨场景三样板验证（player CM7 / daplink / rtos-qemu）

### 样板筛选失败案例：`fs_block_vfs_demo`

目标：

- `Examples/fs/fs_block_vfs_demo`

操作：

- 同样开启严格模式

结果：

- 构建失败
- 失败原因不是 SSU 规则，而是 Windows/MSVC 下第三方 `material_color_utils` 的兼容问题

结论：

- `fs_block_vfs_demo` 目前不适合作为第二个 SSU 严格模式样板
- 这次失败并不否定 SSU，反而说明“第二目标选择”要避开被其他构建噪声淹没的 target

## 当前阶段的真实判断

SSU 已经完成了以下跃迁：

- 从讨论进入文档
- 从文档进入类型系统
- 从类型系统进入 task 声明
- 从 task 声明进入 registry 识别
- 从 registry 识别进入 scheduler 观测
- 从观测进入至少一个真实 target 的严格模式

这说明 SSU 现在已经可以被视为 Charm 的主线方向，而不再只是候选方案。

## 当前仍未完成的部分

### 1. 覆盖面仍然不够

虽然已有多个真实 task 接入 SSU，但还没有完成更大范围的 task 迁移。

### 2. 调度行为尚未 SSU 化

目前 scheduler “看见了 SSU”，但还没有按 SSU 做更高层次的执行策略统一。
当前仍然是“观测先行”，不是“行为统一已完成”。

### 3. 样板验证阶段已完成，进入迁移扩张阶段

当前已经有三个真实 target 证明了严格模式可行：

- `project/player/stn32h747_HQZY/CM7`
- `Charm-dap`（原 `project/daplink`）
- `kernel/rtos/qemu`

下一步重点从“样板数量”转向“高价值 task 迁移覆盖与提交纪律落地”。

## 下一步建议

### P0：第三个干净样板（已完成）

第三个严格模式样板已建立，P0 关闭。

### P1：继续补齐常见 task 的 `ssu_meta()`

优先覆盖：

- pump task
- reactor 驱动 task
- service task

### P2：开始定义“SSU 提交路径”

在现有 event-submit 基础上，逐步收口：

- `event-submit`
- `io-ready-submit`
- `demand-submit`

### P3：后续再讨论行为统一

在覆盖面和样板更稳之前，不急着把 scheduler 重构成完全的 SSU 调度器。

## 一句话总结

SSU 已经不是 Charm 的提案。
它已经成为 Charm 主线中一条正在成形、并且已进入编译事实与运行时观测面的系统语义主轴。






## Phase 2 看板

- `docs/system/ssu_phase2_board.md`

- 阈值可配置：`KernelConfig::ssu_demand_warn_permille`、`KernelConfig::ssu_demand_err_permille`。

- 阈值默认值：`warn=50`（5%）、`err=10`（1%）。
- 可按 target 覆写：例如 `Examples/kernel/windows/main_m3.cpp` 使用 `warn=300`、`err=100` 以放大低 demand 占比告警。
