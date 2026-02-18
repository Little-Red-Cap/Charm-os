# UI/Ink 输入栈分层决策（初次耦合阶段）

## 背景

你当前正在收敛 `Modules/ui/ink/platform/input/*`，并遇到边界问题：
“输入模块是否应该归 HAL 管”。

结合现有架构红线（Foundation → Runtime → Domains）以及你当前“先耦合再治理”的策略，
这里给出一个可落地、低返工的分层方案。

## 结论（先说答案）

**不要把整个 `ui/ink/platform/input` 直接下沉到 HAL。**

应拆成两层：

1. **设备输入采样层（Runtime/IO）**：归 `Modules/io/hal`（或 `io/port`）
   - 负责：GPIO/ADC/编码器原始采样、中断触发、去抖、时间戳
   - 输出：统一的 `RawInputEvent`

2. **UI 输入语义层（Domains/UI）**：保留在 `Modules/ui/ink`
   - 负责：意图识别（click/long-press/scroll）、焦点导航、UI 语义事件
   - 输出：`UiInputIntent` / `UiEvent`

> 一句话：**HAL 只管“读到什么”，UI 才管“这是什么意思”。**

## 为什么不是“全放 HAL”

如果把意图识别、导航策略、控件交互语义都放 HAL，会出现：

- HAL 被 UI 领域语义污染（违反 Runtime 不依赖 Domains）
- 后续换一套 UI（例如 Ink 与 Vivid）时，HAL 无法复用
- 输入策略迭代必须触碰底层，回归成本飙升

## 推荐职责切分（按你现有文件名映射）

### 建议下沉到 Runtime/IO 的内容

可考虑从 `Modules/ui/ink/platform/input/` 中迁移或抽象：

- `input.raw.cppm`：原始采样接口与设备读数
- `input.sampler.cppm`：采样时序、去抖、聚合

并在 `Modules/io/hal`（或 `io/port`）形成统一输入能力：

- `hal_input`（新模块，建议）
- 与 `hal_irq` / `hal_time` 联动，生成时间戳一致的 `RawInputEvent`

### 建议留在 UI/Ink 的内容

继续保留在 `ui/ink/platform/input` 或上提到 `ui/ink/ui/semantics`：

- `input.intent.cppm`：意图解析（短按/长按/旋钮加速等）
- `input.events.cppm`：UI 事件派发
- `input.queue.cppm`：UI 事件队列（可回收到 `service_ring_buffer`）
- `input.encoder_decoder.cppm`：若包含 UI 语义，保留在 UI；若仅做硬件编码解码，可拆出 Runtime 版本

## 初次耦合阶段的迁移策略（最小风险）

### Phase 1：接口先行，不搬文件

- 定义 `RawInputEvent`（Runtime 边界类型）
- 在 UI 输入链路前加一层适配器：`RawInputEvent -> UiInputIntent`
- 现有 UI 逻辑不变，仅替换入口

### Phase 2：替换依赖，不删旧实现

- UI/Ink 改为依赖新的 Runtime 输入接口（HAL/Port）
- 保留旧 `input.raw/input.sampler`，但停止新增调用

### Phase 3：稳定后再清理

- 做最小回归：
  - 编译通过
  - 编码器旋转/按键短按/长按路径验证
- 稳定后再删除重复实现

## 事件模型建议（和 EDA 对齐）

为减少重复造轮子，建议把输入管线显式接到 `system/kernel/eda`：

- ISR/采样层产生 `RawInputEvent`
- 通过无阻塞队列投递（满即丢，记录 trace）
- UI 任务在 EDA 事件处理中完成意图识别与语义分发

这样可以复用你最强的状态机/调度能力，同时保持 HAL 纯净。

## 你现在可以立刻执行的 5 件事

1. 新增 `hal_input` 的最小接口（先 Win/Stub 实现）
2. 把 `input.raw` 的直接硬件读取改成调用 `hal_input`
3. 在 `input.intent` 前引入 `RawInputEvent` 适配层
4. `input.queue` 改用 `core/service/service_ring_buffer`（替换使用，不删旧 API）
5. 给输入链路加 trace 点（丢事件、去抖命中、意图识别结果）

## 协作同步约定（避免并行开发冲突）

你提到会随时修改代码，这里约定一个最小同步流程，后续我按这个流程执行：

1. **任务开始前**先检查最近提交：`git log --oneline -n 8`
2. **改动前**确认工作树干净：`git status --short`
3. **提交前**再次检查最新 `HEAD`，避免基于旧状态继续改
4. **若出现冲突**，优先保留你最新提交的意图，再调整我的改动
5. **每次交付说明**都会附上我执行过的同步命令，确保可追溯

> 实操原则：默认以仓库当前 `HEAD` 为唯一真源，不依赖历史对话中的代码快照。

---

如果你同意，我下一步可以直接给你出一版“文件级迁移清单”（精确到每个 `.cppm` 的去留与依赖方向），并按你当前正在整理的 `Modules/ui/ink/platform/input` 先落第一刀。
