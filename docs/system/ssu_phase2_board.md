# SSU Phase 2 看板（任务-验收-风险）

本看板只服务于 Phase 2（收敛执行），目标是让 SSU 成为新增执行协作的默认落点。

## 本周主目标

- 拿下第二个 SSU 严格模式干净样板（P0）。
- 同步推进 run_loop/phase 提交来源可审计化。
- 将 submit discipline 变成 PR 默认检查动作。

## 任务-验收-风险

### 任务 A：第二个严格模式样板

- **任务**：在低噪声 target 上开启 `CHARM_KERNEL_REQUIRE_SSU_META=1`，确保可执行目标与 `Charm-os` 同时通过。
- **验收**：构建失败只能归因于 SSU 声明缺失或 SSU 路径问题，不允许被第三方噪声主导。
- **风险**：样板选择被 UI/第三方库污染，导致问题定位偏离 SSU 主线。

### 任务 B：run_loop/phase 提交来源审计

- **任务**：为 run_loop step 标注 submit projection（`event-submit` / `io-ready-submit` / `demand-submit`）。
- **验收**：关键 loop 入口具备来源标注，审计文档可追踪；不改变现有执行行为。
- **风险**：只加标签不做审计闭环，后续新增 step 仍可绕过映射检查。

### 任务 C：submit discipline 机制化

- **任务**：PR 模板和评审清单要求新增执行路径必须声明 submit 映射。
- **验收**：新增执行路径 PR 至少包含映射说明、预算策略、阻塞约束。
- **风险**：纪律只停留在文档，不进入默认评审动作，旁路继续增长。

### 任务 D：SSU 观测升级（问题导向）

- **任务**：让 scheduler 输出可直接回答的 SSU 概览指标（trigger/budget/blocking/domain 分布、未命名任务计数）。
- **验收**：调试/日志面可快速定位“语义分布失衡”和“未声明路径”，并输出风险等级（`ok`/`warning`/`error`）。
- **风险**：只增加字段不绑定问题，观测数据无法驱动收敛决策。

## 本周不做

- 不推进 scheduler 全面 SSU runtime 化。
- 不扩大到高噪声 target 的全面迁移。
- 不做大规模历史路径清洗（先保主线可复制）。

## 产出物清单

- 第二个严格模式样板构建记录。
- `docs/system/ssu_run_loop_audit.md` 的审计更新。
- `docs/system/ssu_submit_discipline.md` / PR 模板执行记录。
- scheduler SSU 概览输出样例（JSON）。
## 当前进展

- 已在 `Examples/kernel/windows/main_m3.cpp` 接入 `format_event_source_json(...)`、`format_ssu_overview_json(...)` 与 `format_ssu_hotspots_json(...)` 调试输出。
- 可直接在样板运行日志里观察 submit 来源、SSU 语义分布和风险热点（dominant submit / unnamed tasks / no-demand-submit）。

- 当前阈值策略：由 `KernelConfig::ssu_demand_warn_permille` / `KernelConfig::ssu_demand_err_permille` 控制；`unnamed_tasks` 或 `no_demand_submit` 直接 error。
- 默认阈值来自 `KernelConfig`：`warn=50` / `err=10`（permille）。
- 样板可按场景覆写阈值，用于对照验证风险等级输出。

## 本轮执行记录

- 第二个严格模式样板：`Examples/kernel/windows`（`main_m1.cpp`）。
- 严格模式收口：`Examples/kernel/windows/CMakeLists.txt` 对可执行目标与 `Charm-os` 同时开启 `CHARM_KERNEL_REQUIRE_SSU_META=1`。
- 构建命令：`cmake -S Examples/kernel/windows -B Examples/kernel/windows/build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug`，`cmake --build Examples/kernel/windows/build-ninja -j 8`。
- 当前结果：严格模式构建通过；样板已接入 `EventSource/SsuOverview/SsuHotspots` 导出调用。
