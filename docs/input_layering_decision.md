# UI/Ink 输入层分层决策（初次耦合阶段）

关联总览：
- `docs/io_layering_overview.md`（IO 分层与依赖边界）


## 背景

我们正在收敛 `Modules/ui/ink/platform/input/*`，并遇到边界问题：
“输入模块是否应该归 HAL 管？”

结合当前架构红线（Foundation -> Runtime -> Domains）以及“先耦合再治理”的策略，本文给出一个可落地、低返工的分层方案，并补充 VSF 的 HAL 思路作为参考。

## 结论（先说答案）

不要把整个 `ui/ink/platform/input` 直接下沉到 HAL。
应拆成两层：

1. 设备输入采样层（Runtime/IO）：归 `Modules/io/hal`（或 `io/port`）
   - 负责：GPIO/ADC/编码器原始采样、中断触发、去抖、时间戳
   - 输出：统一的 `RawInputEvent`

2. UI 输入语义层（Domains/UI）：保留在 `Modules/ui/ink`
   - 负责：意图识别（click/long-press/scroll）、焦点导航、UI 语义事件
   - 输出：`UiInputIntent` / `UiEvent`

一句话：HAL 只管“读到什么”，UI 才管“这是什么意思”。

## 为什么不是“全放 HAL”

如果把意图识别、导航策略、控件交互语义都放入 HAL，会导致：

- HAL 被 UI 领域语义污染（违背 Runtime 不依赖 Domains）
- 换 UI（Ink <-> Vivid）时 HAL 无法复用
- 输入策略迭代必须触碰底层，回归成本飙升

## VSF 的 HAL 思路（参考）

从 `Draft/vsf/source/hal/README.md` 和 `Draft/vsf/source/hal/driver/common/template/README.md` 可见，VSF 的 HAL 强调：

- 分层结构：
  - `hal/arch`：架构相关底层
  - `hal/driver`：厂商/芯片系列驱动（vendor/series/chip）
  - `hal/driver/common`：统一的外设接口模板
  - `hal/driver/template`：驱动移植模板（HW/IPCore/Emulated 三类）

- 关键分类：
  1) HW 驱动：具体芯片外设
  2) IPCore 驱动：IP 核，供外设驱动调用
  3) Emulated 驱动：模拟外设接口（软件模拟）

- 统一接口优先于平台细节：由模板与宏展开确保“调用方式一致”。

- Rust 参考：
  VSF 在 `Draft/vsf/source/shell/hal/rust-embedded-hal/lib/README.md` 提供了基于 `hal/driver/driver.h` 的 bindgen 流程，表明其 HAL 头文件天然支持 Rust Embedded HAL 风格的绑定。

对 Charm 的启示：

- HAL 负责稳定、可绑定、可移植的“原始能力”，不吸收 UI 语义。
- 模板式接口 + 设备/厂商分层的思路值得借鉴（尤其是 common/template 的统一接口策略）。
- Rust/ETL 兼容层应建立在 HAL 之上，而不是反向侵入 UI/Domain。

## 推荐职责切分（映射现有文件名）

### 建议下沉到 Runtime/IO 的内容

可考虑从 `Modules/ui/ink/platform/input/` 中迁移或抽象：

- `input.raw.cppm`：原始采样接口与设备读数
- `input.sampler.cppm`：采样时序、去抖、聚合

并在 `Modules/io/hal`（或 `io/port`）形成统一输入能力：

- `hal_input`（新模块）
- 与 `hal_irq` / `hal_time` 联动，生成一致的 `RawInputEvent`

### 建议保留在 UI/Ink 的内容

继续保留在 `ui/ink/platform/input` 或上提到 `ui/ink/ui/semantics`：

- `input.intent.cppm`：意图解析（短按/长按/旋钮加速等）
- `input.events.cppm`：UI 事件派发
- `input.queue.cppm`：UI 事件队列（可回收到 `service_ring_buffer`）
- `input.encoder_decoder.cppm`：若包含 UI 语义，留在 UI；若仅做硬件编码/解码，可拆出 Runtime 版本

## 初次耦合阶段的迁移策略（最小风险）

### Phase 1：接口先行，不挪文件

- 定义 `RawInputEvent`（Runtime 边界类型）
- 在 UI 输入链路前加一层适配器：`RawInputEvent -> UiInputIntent`
- 现有 UI 逻辑不变，仅替换入口

### Phase 2：替换依赖，不删旧实现

- UI/Ink 改为依赖新的 Runtime 输入接口（HAL/Port）
- 保留旧 `input.raw/input.sampler`，但停止新增调用

### Phase 3：稳定后再清理

- 最小回归：
  - 编译通过
  - 编码器旋转/按键短按/长按路径验证
- 稳定后再删除重复实现

## 事件模型建议（与 EDA 对齐）

为减少重复造轮子，建议把输入管线显式接入 `system/kernel/eda`：

- ISR/采样层产生 `RawInputEvent`
- 通过无阻塞队列投递（满即丢，记录 trace）
- UI 任务在 EDA 事件处理中完成意图识别与语义分发

这样可复用内核的状态机/调度能力，同时保持 HAL 纯净。

## 立即可执行的 5 件事

1. 新增 `hal_input` 的最小接口（含 Win/Stub 实现）
2. 把 `input.raw` 的直接硬件读取改为调用 `hal_input`
3. 在 `input.intent` 之前引入 `RawInputEvent` 适配层
4. `input.queue` 改用 `core/service/service_ring_buffer`（替换使用，不删旧 API）
5. 给输入链路加 trace 点（中断事件、去抖命中、意图识别结果）

## 协作同步约定（避免并行冲突）

1. 任务开始前检查最近提交：`git log --oneline -n 8`
2. 改动前确认工作树干净：`git status --short`
3. 提交前再次检查最新 HEAD，避免基于旧状态继续修改
4. 出现冲突优先保留你的最新意图，再调整我的改动
5. 每次交付附上执行过的同步命令，确保可追溯

---

如果你同意，下一步我可以给出“文件级迁移清单”（精确到每个 `.cppm` 的去留与依赖方向），并按你当前整理的 `Modules/ui/ink/platform/input` 先落第一刀。
