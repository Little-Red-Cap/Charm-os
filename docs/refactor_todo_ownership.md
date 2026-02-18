# 大规模耦合重构：推进 TODO List 与分工说明（协作版）

> 目标：在“先耦合、后治理”的阶段，尽量减少重复造轮子，降低冲突概率，并保持可持续推进节奏。

## 0. 执行原则（对齐现有架构）

- 依赖方向保持单向：`Foundation -> Runtime -> Domains`
- 先“替换使用”，再“删除旧实现”
- 每条任务至少有一个最小回归（编译 + 行为点）
- 冲突优先保留“当前 HEAD 的最新意图”

---

## 1. 分工模型（建议）

为减少互相踩文件，采用“轨道制 + 接口握手”：

- 轨道 A（User）：UI/Ink 输入整理与语义层治理
- 轨道 B（AI）：Runtime 侧输入能力对接（HAL/Port/EDA 接入）

接口握手边界：

- A 对 B 的输入：`RawInputEvent` 字段需求
- B 对 A 的输出：稳定的 `hal_input` 最小接口

只要接口握手稳定，A/B 可并行推进，不必等待彼此内部重构完成。

---

## 2. 推进 TODO（按优先级）

### P0（本周必须完成）

**T1. 定义统一输入边界类型**
- 内容：定义 `RawInputEvent`（设备/动作/时间戳/值域）
- 责任：User（草案）+ AI（确认字段）
- 产出：类型定义 + 字段说明
- 完成标准：UI 与 HAL 均可无歧义使用

**T2. 建立 `hal_input` 最小接口（Win/Stub 先通）**
- 内容：硬件采样入口收敛到 Runtime
- 责任：AI
- 产出：接口 + Win/Stub 实现
- 完成标准：UI 可通过接口读取 raw 输入

**T3. UI 输入链路加适配层**
- 内容：`RawInputEvent -> UiInputIntent`
- 责任：User
- 产出：适配函数/模块，旧行为不变
- 完成标准：UI 输入回归通过

**T4. 队列实现收敛**
- 内容：`input.queue` 替换为 `service_ring_buffer`
- 责任：AI（替换方案）+ User（接入验证）
- 完成标准：行为一致；满队列策略明确（满即丢 + trace）

**T5. 输入链路 Trace 点收敛**
- 内容：统一埋点（采样/去抖/丢事件/意图识别）
- 责任：User 主导，AI 补 Runtime 埋点
- 完成标准：问题可通过 trace 复盘路径

### P1（下一阶段）

**T6. encoder_decoder 拆分判定**
- 内容：硬件解码与 UI 语义解码分离
- 责任：User
- 完成标准：硬件相关留 Runtime，语义相关留 UI

**T7. EDA 事件接入**
- 内容：采样层事件进入 EDA 非阻塞队列
- 责任：AI
- 完成标准：UI 任务在 EDA 回调消费输入事件

**T8. 历史重复实现清单化**
- 内容：标注“保留/弃用/待删”的旧输入实现
- 责任：AI 整理，User 确认
- 完成标准：有明确清单与删除前置条件

---

## 3. 文件级分工建议（第一版）

User 优先负责：
- `Modules/ui/ink/platform/input/input.intent.cppm`
- `Modules/ui/ink/platform/input/input.events.cppm`
- `Modules/ui/ink/platform/input/input.encoder_decoder.cppm`（语义部分）
- `Modules/ui/ink/ui/semantics/*`

AI 优先负责：
- `Modules/io/hal/*`（新增 `hal_input`）
- `Modules/io/port/*`（若输入能力经 port 暴露）
- `Modules/system/kernel/eda.cppm`（输入事件接入点）
- `Modules/core/service/service_ring_buffer.cppm` 适配

共享高风险文件（短时冻结）：
- `Modules/ui/ink/platform/input/input.queue.cppm`
- `Modules/ui/ink/platform/input/input.raw.cppm`
- 聚合模块（如 `charm.ui.ink.cppm`）

---

## 4. 冲突最小化机制

**4.1 任务认领卡（轻量）**

每项任务开始前发一条认领信息（commit message 或沟通里）：
- `Task`: T3
- `Files`: `Modules/ui/ink/platform/input/input.intent.cppm`
- `TTL`: 4h
- `Goal`: RawInputEvent 适配层，保持行为不变

**4.2 高风险文件冻结窗口**
- 只允许认领者修改
- 窗口结束立即合并
- 冻结期禁止无关重构

**4.3 提交粒度规则**
- 一次提交只做一类变更（接口/迁移/清理）
- 禁止“接口修改 + 大规模格式化”混在同一提交
- 提交信息标注任务号（T3/T4）

---

## 5. 每日节奏（建议）

- 开始前：`git log --oneline -n 8` + `git status --short`
- 开发中：按任务卡推进，超时即释放
- 提交前：同步最新 HEAD，解决冲突后再提交
- 结束时：更新本文档任务状态（TODO -> DOING -> DONE）

---

## 6. 任务状态看板（维护版）

| 任务 | 描述 | 责任人 | 状态 | 备注 |
|---|---|---|---|---|
| T1 | 定义 RawInputEvent | User + AI | TODO | 先定字段再定实现 |
| T2 | 建立 hal_input 最小接口 | AI | TODO | Win/Stub 先通 |
| T3 | Raw -> Intent 适配 | User | TODO | 保持现有行为 |
| T4 | input.queue 收敛 ring_buffer | Shared | TODO | 满队列策略要定死 |
| T5 | Trace 点收敛 | Shared | TODO | 便于故障复盘 |
| T6 | encoder_decoder 拆分判定 | User | TODO | 硬件/语义分离 |
| T7 | 输入事件接入 EDA | AI | TODO | 非阻塞队列 |
| T8 | 历史重复实现清单 | Shared | TODO | 稳定后再删 |

---

## 7. 额外建议

- 增加任务认领 TTL，避免长时间占坑
- 高风险文件短冻结，降低合并成本
- 提交按任务号切分，提高可审阅性
