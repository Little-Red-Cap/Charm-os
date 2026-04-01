# Charm 的“可调度语义单元（SSU）”可行性与落地路线深研报告

## Executive Summary
SSU 在 Charm 中**高度可行**：仓库已存在多套“执行语义”微体系（EDA 调度、io.reactor+Pump、RunLoop phase、media.pipeline tick/pull、音频 DMA pull），其共同核心并非 Event/DataFlow，而是“可调度的执行 + 明确上下文 + 有界预算”。本报告提出 SSU 最小契约（上下文/触发/预算/组合/观测），给出 C++23 编译期表达方式、跨子系统压测式验证用例，以及 3 阶段（3–6/6–12/12+ 月）渐进迁移路线与风险缓解。

## 关键文件清单

下表为已通过 GitHub 连接器（Little-Red-Cap/Charm-os）检索得到、并将在报告中引用的关键文件（按“执行模型/契约/装配/跨域原语”分组）。fileciteturn61file0L1-L1

| 主题 | 路径 |
|---|---|
| 总体架构红线与收敛策略 | `docs/architecture_overview.md` fileciteturn61file0L1-L1 |
| EDA 任务模型与 TaskRegistry | `Modules/system/kernel/eda.cppm` fileciteturn62file0L1-L1 |
| 事件定义（EventId/Event） | `Modules/system/kernel/evt.cppm` fileciteturn63file0L1-L1 |
| 内核调度器（队列/定时器/trace/stats） | `Modules/system/kernel/scheduler.cppm` fileciteturn64file0L1-L1 |
| 事件队列 RingQueue 后端 | `Modules/system/kernel/event_queue.cppm` fileciteturn65file0L1-L1 |
| 事件队列双后端策略说明 | `Modules/system/kernel/docs/event_queue_backends.md` fileciteturn66file0L1-L1 |
| io.reactor 实现与 waker 绑定 | `Modules/io/reactor/io.reactor.cppm` fileciteturn67file0L1-L1 |
| io.reactor 硬契约 | `docs/io/io_reactor_contract.md` fileciteturn68file0L1-L1 |
| reactor→EDA 的 Pump 适配器 | `Modules/system/reactor/system_reactor_pump.cppm` fileciteturn69file0L1-L1 |
| RunLoop（io/update/render/idle） | `Modules/system/loop/system_run_loop.cppm` fileciteturn70file0L1-L1 |
| media.pipeline 概念接口 | `Modules/media/stream/stream_pipeline.cppm` fileciteturn71file0L1-L1 |
| media.stream Source/Filter/Sink | `Modules/media/stream/stream_source.cppm` fileciteturn72file0L1-L1 / `stream_filter.cppm` fileciteturn73file0L1-L1 / `stream_sink.cppm` fileciteturn74file0L1-L1 |
| AV pipeline 对齐与实时/非实时分层 | `docs/system/av_pipeline_overview.md` fileciteturn75file0L1-L1 |
| Audio v0 架构（DMA 驱动、Pull Engine、控制/数据面） | `docs/system/charm_audio_architecture.md` fileciteturn76file0L1-L1 |
| Audio 设计草案（回调/ISR 约束、Pull-Sim、事务化重配） | `Modules/media/audio/audio_design.md` fileciteturn77file0L1-L1 |
| init.node（cap id/phase/runlevel） | `Modules/core/init/init.node.cppm` fileciteturn78file0L1-L1 |
| init.graph 实现（Kahn topo、无动态分配、相位约束） | `Modules/core/init/init.graph.cppm` fileciteturn89file0L1-L1 |
| init.graph 硬契约 | `docs/system/init_graph_contract.md` fileciteturn79file0L1-L1 |
| 内核能力端口边界（TimeSource/IrqGuard/Wakeup/SwiTrigger） | `Modules/system/kernel/capabilities.cppm` fileciteturn80file0L1-L1 |
| CoreSystemChain（core 底座装配：clock/registry/reactor/EDA/pump） | `Modules/system/init/system_core_chain.cppm` fileciteturn87file0L1-L1 |
| IO Registry（端点注册/发现 + init 绑定） | `Modules/io/registry/io.registry.cppm` fileciteturn96file0L1-L1 |
| io.channel 硬契约（非阻塞、等待交给 Reactor/EDA） | `docs/io/io_channel_contract.md` fileciteturn86file0L1-L1 |
| RTOS 长期硬契约（ISR 语义、生命周期、可观测性） | `docs/system/rtos_longterm_contract.md` fileciteturn83file0L1-L1 |
| 依赖白名单与构建期入口约束 | `docs/architecture/dependency_whitelist.md` fileciteturn81file0L1-L1 |
| UI Kernel 硬契约（Action 化、提交点、公共导出控制） | `docs/ui/ui_kernel_contract.md` fileciteturn92file0L1-L1 |
| 跨域资源句柄原语 | `Modules/core/service/handle_table.cppm` fileciteturn94file0L1-L1 |
| 能力地图索引（EDA/Reactor/Registry/InitGraph 均已列为能力） | `docs/capability_map.md` fileciteturn82file0L1-L1 |
| C++ 编码与架构规约（concept/policy、禁阻塞与依赖纪律） | `docs/project/standards/项目C++编码要求.md` fileciteturn84file0L1-L1 |

## 现状证据摘录

### 执行模型如何实现与交互

Charm 现状不是“缺入口”，而是**并行存在多套执行语义**：EDA 事件驱动调度、Reactor 的 ISR-safe 入队与 task-context drain、RunLoop 的 phase 驱动、Media pipeline 的 tick/pull、Audio 的 DMA 节拍 pull（并强制控制/数据面分离）。这些体系的共同母题是“执行语义”，并且仓库已经用硬契约把关键规则固化下来（如 non-blocking、禁止 busy-spin、ISR 内不做重活）。fileciteturn61file0L1-L1 fileciteturn86file0L1-L1

下面的表格给出“现状证据摘录”：每条都对应到文件/要点/引用。

| 子系统 | 关键要点（证据摘录/转述） | 证据文件 |
|---|---|---|
| 统一架构红线 | 设计原则明确要求：能力走 `init.graph` 装配、Channel 必须 non-blocking、协议层禁止 busy-spin/自带超时、禁止隐式全局入口、依赖只允许单向向上。 | `docs/architecture_overview.md` fileciteturn61file0L1-L1 |
| EDA 任务模型 | `EdaTask` concept：要求 `Task::priority` 与 `task.on_event(evt)`；`TaskRegistry` 编译期生成 id/优先级表，并负责 `dispatch`。 | `Modules/system/kernel/eda.cppm` fileciteturn62file0L1-L1 |
| Event 语义 | `EventId` 既含系统语义（init/tick/sync/terminate），也含跨子系统泵（reactor_drain/input_pump/canopen_pump）。这说明“事件”在系统中承担“驱动执行”的角色。 | `Modules/system/kernel/evt.cppm` fileciteturn63file0L1-L1 |
| Scheduler 的“执行收口能力” | Scheduler 内置：事件过滤链（dedup/debounce/rate-limit/boost/coalesce）、IRQGuard 保护入队、soft trigger + wakeup、`run_budget()` 有界执行、trace/stats/JSON 导出与 replay。 | `Modules/system/kernel/scheduler.cppm` fileciteturn64file0L1-L1 |
| 事件队列策略 | RingQueue 后端支持容量/丢弃策略与 coalesce/cancel/drop；并有“双后端策略”：需要动态优先级时切换到 ListQueue。 | `kernel/event_queue.cppm` fileciteturn65file0L1-L1；`event_queue_backends.md` fileciteturn66file0L1-L1 |
| Reactor 执行语义 | Reactor 明确区分：`notify()` ISR-safe、只入队合并并触发 waker；`drain()` 必须 task context，消费 pending 并 dispatch callbacks。 | `io.reactor.cppm` fileciteturn67file0L1-L1；契约 `io_reactor_contract.md` fileciteturn68file0L1-L1 |
| Reactor Pump（跨模型适配） | `ReactorPumpTask` 作为 EDA task，仅订阅 `reactor_drain`，`on_event` 中 `drain(budget)`，若 more 则自我 post 再执行；waker 只是 post 一次 `reactor_drain` 事件。 | `system_reactor_pump.cppm` fileciteturn69file0L1-L1 |
| RunLoop 执行语义 | RunLoop 用 `LoopPhase{io,update,render,idle}` 组织每轮执行；且提供 add_scheduler_step/add_reactor_step，把 Scheduler/Reactor 作为 loop step 执行。 | `system_run_loop.cppm` fileciteturn70file0L1-L1 |
| Media pipeline 执行语义 | `media.pipeline` 抽象要求 `start/stop/tick`；同时 `StreamSink` 用 `FillCallback` pull 数据（需求驱动），这与“事件触发”不同。 | `stream_pipeline.cppm` fileciteturn71file0L1-L1；`stream_sink.cppm` fileciteturn74file0L1-L1 |
| AV 实时/非实时分层 | 文档明确：实时路径（回调/ISR）只做 FIFO 拉取+补零+计数；非实时路径（事件线程）做读/解码/变换/写 FIFO，并提出抽到 `media/stream` 的统一接口层。 | `av_pipeline_overview.md` fileciteturn75file0L1-L1 |
| Audio v0：设备时钟主导 | 文档强调：设备时钟驱动（DMA IRQ），Graph 必须 pull；IRQ 只做最短路径不 decode；控制面/数据面分离（Actor vs DSP Graph）。 | `charm_audio_architecture.md` fileciteturn76file0L1-L1 |
| Audio 约束更“硬” | 音频设计要求 FillCallback 可能在 ISR/实时线程执行，禁止调用 decoder/graph/storage、禁止分配/加锁；同时提供 Pull-Sim 在 PC 无硬件下复现实时 pull 语义用于回归。 | `audio_design.md` fileciteturn77file0L1-L1 |
| 装配：init.graph | init.graph 以固定数组存图，build 进行 topo sort，要求 capability 唯一、phase 不逆序、start 按拓扑 init（init 内不阻塞）。 | `init.graph.cppm` fileciteturn89file0L1-L1；`init_graph_contract.md` fileciteturn79file0L1-L1 |
| CoreSystemChain 把底座串起来 | CoreSystemChain 显式把 `system.clock`、`io.registry`、`io.reactor`、`kernel.eda`、`reactor_pump` 等 node 纳入同一装配链。 | `system_core_chain.cppm` fileciteturn87file0L1-L1 |
| “端口边界”已抽象 | Scheduler 只要求 `Caps` 满足 TimeSource/IrqGuard/Wakeup/SwiTrigger 概念，暗示 RTOS/平台层可作为 executor backend 提供这些能力。 | `kernel/capabilities.cppm` fileciteturn80file0L1-L1 |
| IO Registry 能力注册/发现 | Registry 支持按 name/cap id 注册/替换/打开 Channel endpoint，并以 `RegistryBinding` 作为 init.node 提供能力。 | `io.registry.cppm` fileciteturn96file0L1-L1 |
| io.channel = 非阻塞契约 | Channel 必须 fully non-blocking；等待/超时必须交给 Kernel（Reactor/EDA）；协议层禁止 wait loop/busy-spin。 | `io_channel_contract.md` fileciteturn86file0L1-L1 |
| RTOS 长期契约：ISR/生命周期/观测 | 强制两阶段（Startup registration + Runtime activation），生产构建禁止 runtime 动态创建；ISR API 必须显式命名并只做 mark/递延唤醒；可观测性（trace/stats）“必须存在”。 | `rtos_longterm_contract.md` fileciteturn83file0L1-L1 |
| 构建期依赖纪律 | 依赖白名单用于构建期阻断“被移除的入口模块”导入，违规视为构建失败；与整体“依赖红线”配合。 | `dependency_whitelist.md` fileciteturn81file0L1-L1 |
| UI 也在做“执行语义收口” | UI Kernel 契约要求：输入阶段只产出 Action、状态写入只能在统一提交点发生、public 聚合不得 re-export internal/private；违规直接构建失败。 | `ui_kernel_contract.md` fileciteturn92file0L1-L1 |
| 跨域资源句柄原语 | `HandleTable` 提供 index+generation 的句柄分配/校验/回收，这类“可回收句柄”非常适合作为 SSU 中资源引用的统一形态。 | `service/handle_table.cppm` fileciteturn94file0L1-L1 |
| 代码规约支持“零成本强约束” | 明确强调：MCU 实时路径禁用动态分配/虚表/阻塞；优先 `concept + template`；协议等待必须走 Kernel/EDA+Reactor；能力装配必须走 init.graph。 | `项目C++编码要求.md` fileciteturn84file0L1-L1 |

### 现状交互关系图

下面用一个简化关系图把“多套执行模型如何互相缝合”呈现出来（你要做 SSU 的关键，就是把这些缝合点升格成**标准化的‘可调度单元’契约**）。

```mermaid
flowchart TD
  subgraph Boot["init.graph / CoreSystemChain"]
    IG[init.graph topo build/start]
    CS[CoreSystemChain: clock/registry/reactor/eda/pump]
    IG --> CS
  end

  subgraph Kernel["Kernel: EDA + Scheduler"]
    EVT[kernel::EventId/Event]
    EDA[EdaTask + TaskRegistry]
    SCH[Scheduler: queues/timers/trace/stats]
    EVT --> EDA --> SCH
  end

  subgraph IO["IO: Channel + Reactor"]
    CH[io.channel: non-blocking]
    R[io.reactor: notify(enqueue) / drain(dispatch)]
  end

  subgraph Bridge["Bridge: ReactorPump"]
    PUMP[ReactorPumpTask (EDA task)\nreactor_drain -> drain(budget)\nmore? self-post]
  end

  subgraph Loop["RunLoop"]
    RL[RunLoop phases: io/update/render/idle]
    RL -->|step| SCH
    RL -->|step| R
  end

  subgraph Media["Media"]
    PIPE[media.pipeline: start/stop/tick]
    SINK[StreamSink FillCallback (pull)]
    AUDIO[Audio v0: DMA-driven pull engine]
  end

  CH --> R --> PUMP --> SCH
  PIPE --> SINK
  AUDIO --> SINK
```

## 对标要点

本节对比 Linux / Zephyr / QP（Quantum Leaps）如何把“执行语义”收敛为可调度单元，并提炼可借鉴点。为保证权威性，本节优先引用官方/权威文档（kernel.org、docs.kernel.org、docs.zephyrproject.org、state-machine.com、Quantum Leaps 官方文档）。

### Linux：workqueue、BH/softirq 与 threaded IRQ 的“上下文显式化”

Linux workqueue 文档将 workqueue 定义为“异步执行上下文”：把要执行的函数封装进 work item，入队后由 worker 线程按序执行；并明确 work item 可以在“线程上下文”或 “BH（softirq）上下文”执行。citeturn0search0  
这对 SSU 的启示是：**把执行域（thread vs softirq/BH）作为抽象的一部分**，否则系统会在 ISR/线程之间长出隐式旁路。

Linux 的 generic IRQ 文档解释了 threaded IRQ 的拆分：primary handler 仍在 hard interrupt context 里执行，只负责判断来源并返回 `IRQ_WAKE_THREAD`；随后由 irq handler thread 执行 `thread_fn` 进行后续处理。citeturn0search1  
这与 Charm 已经在 reactor 上实践的“ISR-safe notify + task-context drain”高度同构：把“不可阻塞的最短路径”与“可调度的后续执行”分离，并用统一机制连接两者。fileciteturn68file0L1-L1

可借鉴要素（压缩为可执行的设计点）：
- 将执行上下文作为 SSU 第一公民（hardirq/BH/thread 对应 Charm 的 isr_safe/task_only/anywhere）。
- 将“延后执行”机制产品化：work item 是可调度单元（可取消/可 flush/可链式 resubmit），而不是散落的 callback 约定。citeturn0search0
- 为“禁止睡眠/禁止阻塞”的执行域提供显式语义（Linux BH work 不能 sleep）。citeturn0search0

### Zephyr：workqueue 与 device driver model 的“系统化收口”

Zephyr workqueue 文档明确：workqueue 由专用线程按 FIFO 处理 work item；典型用途是 ISR 或高优先级线程把非紧急处理 offload 到低优先级线程，避免影响时序敏感路径；work handler 运行在线程上下文，但需要谨慎处理潜在阻塞，否则会阻塞队列后续 work。citeturn1search0  
这与 Charm 的 reactor contract（回调必须 budgeted、不得阻塞，drain 必须由 EDA 驱动）形成直接对照。fileciteturn68file0L1-L1

Zephyr device driver model 文档强调：device model 负责初始化系统中配置的 drivers，并为每类 driver 提供一致的 type API；初始化按 level 顺序组织。citeturn1search1  
对 Charm 而言，init.graph 的 Phase/Runlevel 与 “capability 唯一提供者”机制，已经具备类似驱动初始化组织能力，只是需要把“执行语义单元（SSU）”与“能力装配（capability）”之间的接口做成标准。fileciteturn89file0L1-L1 fileciteturn79file0L1-L1

可借鉴要素：
- work item 生命周期管理（可 resubmit、可 cancel、可 wait/flush），把“分段执行”作为一等能力。citeturn1search0
- 用系统对象（workqueue）集中承载调度与线程资源，避免每个子系统自造“事件线程/泵链路”。

### QP：Active Object + Run-to-Completion + No Blocking 的“语义铁律”

QP 的 Active Object 文档把 AO 定义为：拥有 event queue 与 execution context 的自治对象；AO 必须 run-to-completion（RTC）地逐个处理事件；并明确指出 blocking 与 RTC 语义不兼容，因为 blocking 会引入“背门事件”，破坏 RTC。citeturn2search1  
QP/C++（QV scheduler）文档进一步解释：调度器总是选择“最高优先级且队列非空”的 active object，取出事件并 dispatch，状态机 RTC 后返回调度器循环；并强调可通过“自发事件”把长处理拆短。citeturn2search2

可借鉴要素（对 Charm/SSU 特别关键）：
- 把 RTC 与“禁止阻塞”写进 SSU 的最小契约，而不是靠代码评审口头约定。citeturn2search1
- 把“分段执行”变成标准手法（self-post / resubmit），这与你的 ReactorPump“more→self post”完全一致。fileciteturn69file0L1-L1 citeturn2search2
- 把事件队列的“单消费者、多生产者”语义固化（AO queue 单 consumer；生产者可来自 ISR 或其他组件）。citeturn2search1

## SSU 最小契约规范

### 为什么 SSU 是“更贴合 Charm 的核心抽象”

在 Charm 仓库里，“执行语义”已经在多个地方被硬化：
- 协议等待/超时不能在协议层实现，必须交给 Kernel（Reactor/EDA）。fileciteturn86file0L1-L1
- Reactor: notify ISR-safe 只入队；drain 在 task context，由泵任务驱动；回调必须 budgeted、禁止 busy-spin/睡眠/内部超时。fileciteturn68file0L1-L1
- Audio: DMA 节拍驱动，IRQ 内禁止 decode/malloc/logging/锁竞争，控制/数据面分离，Graph pull。fileciteturn76file0L1-L1 fileciteturn77file0L1-L1
- RTOS long-term: ISR deferral 必须集中、生命周期两阶段、可观测性必须存在。fileciteturn83file0L1-L1

这说明 Charm 的“系统复杂度坍塌点”更可能来自：**把多套执行规则收敛成一种可组合、可调度、可约束的执行基本构件**，而不是把 event 或 stream 作为万物抽象。

### SSU 最小可行契约

SSU（Schedulable Semantic Unit）建议定义为一个“可被调度的语义执行体”，具备以下最小字段与规则。每一项都能在现有 Charm 体系中找到对应落点（因此不是空想）。

#### 执行上下文
目标是让“在哪里能运行”成为编译期可见信息，而不是文档备注。

- `ExecContext::isr_only`：只允许 ISR/硬实时回调（例如音频 FillCallback 的最短路径部分）。fileciteturn77file0L1-L1
- `ExecContext::task_only`：只允许任务上下文（例如 reactor 的 drain）。fileciteturn68file0L1-L1
- `ExecContext::anywhere`：工具/PC/非关键路径可以放宽（但在 MCU 生产构建仍可收紧，符合 RTOS contract 的“debug profile 才允许动态行为”理念）。fileciteturn83file0L1-L1

#### 触发语义
SSU 应统一表达“谁在什么时候需要我执行”，而不是只表达“发生了什么”。

- `Trigger::event`：来自 kernel event queue（EDA）。fileciteturn62file0L1-L1
- `Trigger::io_ready`：来自 reactor notify（ISR-safe 入队）→ pump → 事件驱动 drain。fileciteturn67file0L1-L1 fileciteturn69file0L1-L1
- `Trigger::timer`：来自 scheduler 定时器 schedule_at → tick → post。fileciteturn64file0L1-L1
- `Trigger::frame`：来自 run loop phase tick（UI/render 场景）。fileciteturn70file0L1-L1
- `Trigger::demand`：来自 pull callback（stream sink / audio device）。fileciteturn74file0L1-L1 fileciteturn76file0L1-L1

#### 预算与运行语义
预算是 “强约束藏在零成本抽象里” 的关键：它既是性能/实时性的约束点，也是可组合性的前提（防止一个单元吞掉 CPU，导致系统性饥饿）。

- `RunSemantics::run_to_completion`：单次执行必须在有界时间内返回（与 RTC 对应）。citeturn2search1
- `Budget`：以 bytes/frames/iteration/event_count 任一维度表达；预算耗尽必须“返回 + resubmit/self-post”。Charm 现有 ReactorPump 已是标准范式（more→self-post）。fileciteturn69file0L1-L1
- 禁则：禁止 busy-spin、sleep、内部超时（Reactor contract 与架构总览都要求）。fileciteturn68file0L1-L1 fileciteturn61file0L1-L1

#### 组合接口
组合性决定 SSU 能否成为“系统语言”。

- **线性组合**：A→B→C（典型 pipeline）。`media.stream` 已具备 Source/Filter/Sink 的类型擦除引用，把组合入口做成标准接口。fileciteturn72file0L1-L1 fileciteturn73file0L1-L1 fileciteturn74file0L1-L1
- **并行组合**：多个 SSU 共享同一触发域（如多 IO endpoint 共用 Reactor），由调度器/泵按公平策略推进。
- **跨域组合**：ISR 域的“最短路径 SSU”只能产生“任务域可调度 SSU 的触发/入队”，不能直接跨到重逻辑（与 threaded-IRQ/workqueue 思想一致）。citeturn0search1turn1search0

#### 可观测性
RTOS long-term contract 明确“可观测性必须存在”。fileciteturn83file0L1-L1  
Scheduler 已内置 trace/stats/JSON 输出与 replay，这为 SSU 统一观测提供现成钩子：SSU 只需要把“执行一次”视为 trace record 的基本单位（包含：触发源/预算消耗/耗时/是否 resubmit）。fileciteturn64file0L1-L1

### 用 C++23 编译期特性表达 SSU 契约

Charm 已明确鼓励用 `concept + template` 替代虚表，并在 MCU 路径强调禁动态分配/禁阻塞，适合用编译期契约把“正确路径最轻松”固化下来。fileciteturn84file0L1-L1

下面给出一个“签名片级”的 C++23 契约草案（示意，不依赖具体实现细节）：

```cpp
// SSU Core Contract (C++23 sketch)

namespace ssu {

enum class ExecContext : uint8_t { isr_only, task_only, anywhere };
enum class TriggerKind : uint8_t { event, timer, io_ready, frame, demand };

struct Budget {
  enum class Unit : uint8_t { iterations, bytes, frames, events };
  Unit unit{Unit::iterations};
  uint32_t max{0}; // 0 => "implementation default", but still bounded by policy
};

struct Traits {
  ExecContext context{ExecContext::task_only};
  TriggerKind trigger{TriggerKind::event};
  Budget budget{};
  bool non_blocking{true};      // must be true for production MCU
  bool rtc{true};               // run-to-completion
  bool stateful{false};         // internal state across invocations
};

template <class T>
concept HasTraits = requires {
  { T::traits } -> std::same_as<const Traits&>;
};

template <class T>
concept SchedulableSemanticUnit =
  HasTraits<T> &&
  requires(T& t) {
    // "step" is the universal execution entry.
    // returns whether it should be scheduled again immediately.
    { t.step() } noexcept -> std::same_as<bool>;
  };

// Optional: typed I/O for composition, still zero-cost via concepts.
template <class T>
concept HasInOut = requires {
  typename T::In;
  typename T::Out;
};

} // namespace ssu
```

与 Charm 现有模型的对接方式（关键点）：
- `kernel::EdaTask` 已经是一个“可调度语义单元”的特化：触发=event、上下文=task_only、RTC=强（单次 on_event 返回）。因此 SSU 的第一阶段不需要推翻 EDA，而是**为 EDA 加一层统一 traits**，并提供跨模型适配器。fileciteturn62file0L1-L1
- `ReactorPumpTask` 是“预算化 drain + 自我 resubmit”的标准模板，适合作为 SSU 的“可组合执行策略”样例库。fileciteturn69file0L1-L1
- `RunLoop` 的 step（SchedulerLoopStep/ReactorLoopStep）可以被看作 TriggerKind::frame 的 SSU 执行器入口，后续可统一为同一 SSU executor。fileciteturn70file0L1-L1
- `media.stream` 的 `FillCallback` 明确属于 demand 触发，且可能处于实时/ISR 域（Audio 文档已强调），SSU 必须允许“可调度单元”在不同执行域拆分实现：ISR-only 只做 FIFO 读/补零/计数，task-only 负责 decode/graph/IO。fileciteturn75file0L1-L1 fileciteturn77file0L1-L1

## 压测式验证用例设计

约束/假设（未指定项按要求标注）：
- 是否有可运行测试环境或硬件：未指定；本报告假定只能进行代码/文档级验证与设计验证。若需要运行时压测（latency/jitter/throughput），需要用户提供硬件或 PC bench 环境（例如 Windows 主线、音频 Pull-Sim、或目标 MCU）。fileciteturn77file0L1-L1
- 团队规模与时间预算：未指定；迁移路线按 3–6 人团队、中等优先级估算。

### 用例优先级与总览表

下表给出 3 个“跨子系统原语”压测用例，作为验证 SSU 是否真正带来“复杂度坍塌”的硬指标。它们刻意覆盖：资源句柄/能力装配、事件/生命周期/IO、状态机/会话与实时域桥接。

| 用例 | 跨子系统原语 | 验证目标（压测点） | 推荐优先级 |
|---|---|---|---|
| 用例 A | Resource Handle + Capability Registry | 证明 SSU 能把“资源引用/回收/装配”统一成稳定语义，避免每个子系统自造 handle/registry；并验证端口边界（Caps）能支撑 executor backend。fileciteturn94file0L1-L1 fileciteturn89file0L1-L1 | P0 |
| 用例 B | Event/Message + Lifecycle | 证明 SSU 能统一 EDA + Reactor + Timer + budget 语义，减少“泵链路特例”；并用指标证明在高频 IO 负载下仍可控（drop/filter/latency）。fileciteturn68file0L1-L1 fileciteturn64file0L1-L1 | P0 |
| 用例 C | State/Session（UI Action + Audio Transaction） | 证明 SSU 能承载“控制面状态机 + 数据面实时 pull”跨域协作，确保 ISR 域禁则、事务化重配、以及 UI Action 提交点一致性。fileciteturn92file0L1-L1 fileciteturn77file0L1-L1 | P1 |

### 用例详细定义

#### 用例 A：Resource Handle + Capability Registry
验证目标  
把资源引用统一为“可回收句柄 + capability 装配/发现”，并证明 SSU 不需要吞并 init.graph/registry，而是能以最薄适配接入它们。

输入场景  
在 Windows 主线（或仅编译级）构造一个最小系统链：init.graph 装配 `system.clock`、`io.registry`、`io.reactor`、`kernel.eda`、`reactor_pump`。fileciteturn87file0L1-L1  
在此基础上引入一个“资源服务 SSU”：用 `HandleTable` 分配句柄引用 IO endpoint（Channel/Reactable），并通过 `io.registry` 注册/替换 endpoint。fileciteturn94file0L1-L1 fileciteturn96file0L1-L1

度量指标
- code churn：把一个现有子系统从“自持资源表”迁到 `HandleTable+Registry` 的改动量（文件数/行数/依赖关系变化）。
- API ergonomics：调用方是否只需要“cap id/open + handle”，而不需要触达底层对象（减少直接依赖）——与“禁止隐式全局入口”一致。fileciteturn61file0L1-L1
- correctness：handle generation 失效能否阻止 use-after-free（HandleTable 的 generation 校验）。fileciteturn94file0L1-L1

失败判定
- 任一资源在释放后仍可通过旧 handle 访问成功（generation 失效失败）。
- capability provider 重复/缺失未被 init.graph 在 build 阶段拒绝（违反 unique provider / missing cap 规则）。fileciteturn79file0L1-L1

#### 用例 B：Event/Message + Lifecycle
验证目标  
用 SSU 把 EDA（事件驱动）与 Reactor（IO 就绪驱动）统一为同一种“可调度单元”执行语义，并验证在高频 IO notify 下仍能保持有界执行与可观测性。

输入场景  
构造 3 类触发源：
1) 高频 `io.reactor.notify()`（模拟 UART RX/USB EP 事件）；要求 ISR-safe 入队合并，不运行回调。fileciteturn68file0L1-L1  
2) 由 ReactorPumpTask 以 budget drain，并在 “more” 时自我 post `reactor_drain`。fileciteturn69file0L1-L1  
3) Scheduler 的 timer 触发与 event 过滤链（dedup/debounce/rate-limit/coalesce/boost）。fileciteturn64file0L1-L1

度量指标
- latency：从 notify 到对应协议回调被执行的时间（需要运行环境；若无，则以“事件路径长度/调用层级”作为设计验证 proxy）。
- jitter：drain 间隔波动（RunLoop 或 idle/wakeup 策略影响；Scheduler 支持 `run_idle()` 结合 Wakeup）。fileciteturn64file0L1-L1
- throughput：单位时间内处理的 IO 事件数量（可从 Scheduler stats：posted/dispatched/filtered/dropped、event_posted/event_dispatched 统计获取）。fileciteturn64file0L1-L1
- fairness：在高优先级事件持续涌入时，低优先级事件是否长期饥饿（Scheduler 有 starve_ 状态与多级迭代取队列逻辑，可用于观测/改进）。fileciteturn64file0L1-L1
- lifecycle correctness：禁止 runtime 动态创建 SSU 实体（生产构建只能激活预分配/注册的单元），对齐 RTOS long-term contract。fileciteturn83file0L1-L1

失败判定
- 任何协议回调出现 busy-spin/sleep/内部超时（违反 reactor contract 与架构原则）。fileciteturn68file0L1-L1 fileciteturn61file0L1-L1
- `drain()` 被非 pump 直接调用出现在生产路径（必须通过 lint/CI 与可见性规则阻断）。fileciteturn68file0L1-L1
- dropped 在无拥塞时持续增长或无法解释（需要 trace/stats；可观测性必须存在）。fileciteturn83file0L1-L1 fileciteturn64file0L1-L1

#### 用例 C：State/Session（UI Action + Audio Transaction）
验证目标  
验证 SSU 能否成为“控制面状态机 + 数据面实时 pull”的统一组织方式：UI 输入只产出 Action，状态写入在统一提交点；音频重配事务必须 stop→flush→reopen→prefill→start；实时回调只读 FIFO + 补零 + 计数。fileciteturn92file0L1-L1 fileciteturn77file0L1-L1

输入场景  
- UI 侧：输入阶段产生 Action（不直接写 hover/pressed/focus），提交点统一写入状态。fileciteturn92file0L1-L1  
- Audio 侧：触发一个“会话重配”（例如 FollowInput↔FixedRate 或 sample rate 切换），要求在非实时线程事务化执行；实时 FillCallback/ISR 严格禁调用 decoder/graph/storage、禁分配/加锁。fileciteturn77file0L1-L1  
- 使用 Pull-Sim 在 PC 环境复现 DMA pull + jitter 注入，作为无硬件压测入口。fileciteturn77file0L1-L1

度量指标
- jitter：回调间隔 dt(ms) 的 min/avg/max；underrun=0（音频文档已经给出验收口径）。fileciteturn76file0L1-L1
- code churn：将 Player 状态机/命令队列从“sleep_for 轮询”迁入 “SSU 触发（timer/event）”的改动量（音频文档已指出 MCU 端禁止 sleep，需由 scheduler 定时事件驱动）。fileciteturn77file0L1-L1
- API ergonomics：UI Action → Audio Command 是否能通过统一的 SSU 提交接口完成，而不需要跨层直连内部对象（符合 UI contract 与依赖红线）。fileciteturn92file0L1-L1 fileciteturn61file0L1-L1

失败判定
- 回调/ISR 内出现 decode/graph/IO/锁竞争/日志格式化等行为（违反 Audio 约束）。fileciteturn76file0L1-L1 fileciteturn77file0L1-L1
- UI 输入阶段直接写状态未被构建期阻断（违反 UI contract “守卫”）。fileciteturn92file0L1-L1
- 重配事务中出现“半重配继续播放”或资源失效后仍被回调访问（事务化失败）。fileciteturn77file0L1-L1

## 迁移路线与 RTOS 定位

### RTOS 在 SSU 体系中的定位结论

从仓库现状来看，Charm 已把传统 RTOS 的部分职责抽象为 `kernel::Capabilities`：TimeSource/IrqGuard/Wakeup/SwiTrigger。fileciteturn80file0L1-L1  
这天然支持一种分层定位：

- **Charm/SSU 负责“执行语义层”**：可调度单元、触发语义、预算、过滤链、trace/stats、跨域适配（如 reactor pump）。fileciteturn64file0L1-L1 fileciteturn69file0L1-L1
- **RTOS/平台层负责“执行资源后端（executor backend）”**：上下文切换、IRQ 控制、tick source、唤醒/低功耗、（可选）多线程/多核等；符合 RTOS long-term contract 的 core vs port boundary。fileciteturn83file0L1-L1

因此，“RTOS 被吞没”只会发生在极简配置（bare-metal + 最小能力后端），而在多数工程场景下 RTOS 更现实的角色是 executor backend。

### 渐进迁移路线表

下表分短期（3–6 月）、中期（6–12 月）、长期（12+ 月）三个阶段。团队规模与预算未指定，本表按 3–6 人、优先级中等估算；若优先级更高可压缩周期，但风险会上升。

| 阶段 | 关键任务 | 可交付物 | 风险 | 缓解措施 |
|---|---|---|---|---|
| 短期 | SSU 最小契约落地（不推翻 EDA） | 1) `docs/system/ssu_contract.md`（硬规则） 2) `kernel.ssu`（traits+concept） 3) EDA→SSU adapter（把 EdaTask 视作 SSU 特化） 4) 统一 trace/stats 事件（SSU 执行一次=一条记录） | “新抽象与现有 Node/Graph 命名冲突”导致概念混乱（init::Node、audio Node 已存在）fileciteturn78file0L1-L1 fileciteturn76file0L1-L1 | 统一命名：避免再用 Node；建议用 `Unit/Step/Work`；在文档中明确三类 Node 的语义边界 |
| 短期 | 把 ReactorPump 标准化为 SSU 参考适配器 | 1) `ssu::IoDrainUnit`（模板化 pump：budget + more→resubmit） 2) CI 检查：生产构建禁止直接调用 `reactor.drain()`（除 pump）fileciteturn68file0L1-L1 | Lint/CI 误伤（测试/工具代码） | 使用例外白名单（类似 dependency_whitelist 的 exception 策略），并记录 rationale fileciteturn81file0L1-L1 |
| 短期 | 导出与可见性规则补齐 | 1) 在聚合入口（例如 `charm.system/charm.io`）减少 re-export 内部细节 2) 新增 `SSU entry`：上层只见“提交/组合接口”，不见底层执行细节；对齐“禁止隐式全局入口”。fileciteturn61file0L1-L1 | 上层现有代码依赖大量内部模块，短期 churn 高 | 采取“先替换使用、不立即删除旧 API”的回收策略（架构总览已定义回收流程）fileciteturn61file0L1-L1 |
| 中期 | RunLoop 与 Scheduler/SSU 合流 | 1) `SSU Executor` 支持 Trigger::frame 2) RunLoop steps 改为提交 SSU（而非直接调用 scheduler->run_budget/ reactor->drain）fileciteturn70file0L1-L1 | UI/渲染链对时序敏感，合流可能引入抖动 | 保持 RunLoop phase 外壳不变，只替换 step 内实现；先在 PC/Windows 主线上回归 |
| 中期 | media.pipeline → SSU 适配器 | 1) `PipelineUnit`：tick-driven + demand-driven 双入口 2) 统一把 “FillCallback demand” 表达为 SSU Trigger::demand，并强制 ISR-only 最短路径规则（FIFO read + zero pad + counters）。fileciteturn74file0L1-L1 fileciteturn77file0L1-L1 | 把 demand 和 event 混在一起会导致实时域被污染（decode 误入 ISR） | 强制 SSU 分域：demand 只能触发 task-only 单元（通过 deferred submit），并在文档/静态检查中固化禁则 |
| 中期 | 生命周期与动态创建收敛 | 1) SSU 只允许 “Startup registration + Runtime activation” 两阶段 2) 禁止生产构建 runtime new SSU（对齐 RTOS contract）fileciteturn83file0L1-L1 | 现有 demo/PC 工具依赖动态行为 | 用 profile 分层：PC/debug 放宽，MCU/production 收紧；contract 文档写明适用范围 |
| 长期 | SSU + capability 注册形成“生态适配面” | 1) 第三方库适配面：minimal adapter interface 以 SSU/Channel/Registry 为核心 2) “Charm-compatible” 标准：必须声明触发/上下文/预算/阻塞性 | 标准过重导致生态不愿适配 | 维持适配面极薄：只要求“能被 SSU 调度/观测、能遵守 non-blocking 规则”，其它能力允许渐进 |
| 长期 | RTOS backend 标准化与多目标移植 | 1) `Caps` 实现模板（Cortex-M/RISC-V） 2) 端口边界测试（符合 core vs port boundary）fileciteturn83file0L1-L1 | 多平台差异导致 contract 被架空 | 把 “Caps 合规性” 变成编译期 concept 检查 + 最小运行自检（类似 init.graph_self_check/registry_self_check）fileciteturn89file0L1-L1 fileciteturn96file0L1-L1 |

### 必须修改的导出/可见性规则与 lint/CI 约束建议

这些不是“锦上添花”，而是 SSU 迁移能否成功的强约束闭环。Charm 已经在 dependency whitelist 与 UI kernel guard 上证明“构建失败级约束”是可行路线。fileciteturn81file0L1-L1 fileciteturn92file0L1-L1

建议新增/强化：
- **导出面收口**：在 public 聚合模块中只导出 SSU/提交接口与能力入口，禁止 re-export internal/private（UI contract 已有同款规则，可复用机制）。fileciteturn92file0L1-L1
- **Reactor drain 约束落地为工具规则**：生产构建禁止任何非 pump 的 `drain()` 调用（白名单仅允许 `system_reactor_pump` 或测试 harness）。fileciteturn68file0L1-L1
- **协议层禁 busy-spin**：把 `io_channel_contract` 与 `io_reactor_contract` 的禁则转成 CI grep/lint（例如禁止 `while(would_block)` 模式、禁止 sleep/timeout loops）。fileciteturn86file0L1-L1
- **生命周期两阶段**：对 SSU/服务对象建立 “register vs activate” 模式，生产构建禁 runtime new（RTOS contract 已要求）。fileciteturn83file0L1-L1

## 下一步行动项

1) **写 SSU 合同与命名定稿**（负责人：架构 owner/Tech Lead，预计 2–3 天）  
输出：`docs/system/ssu_contract.md`（参照 `io_reactor_contract`/`init_graph_contract` 写 Hard Rules 风格），并明确 SSU 命名不与 `init::Node`、audio Node 冲突。fileciteturn68file0L1-L1 fileciteturn79file0L1-L1

2) **实现 `kernel.ssu`（traits+concept）与 EDA→SSU 适配器**（负责人：核心库工程师，预计 1–2 周）  
输出：最小 `SchedulableSemanticUnit` concept；为 `EdaTask` 提供 traits（Trigger::event、ExecContext::task_only、RTC=true），不改业务代码先跑通编译链。fileciteturn62file0L1-L1

3) **把 ReactorPump 抽成可复用 SSU 模板**（负责人：系统工程师/IO owner，预计 1 周）  
输出：`ssu::PumpUnit`（budget + more→resubmit），并把 `reactor_drain` 作为标准触发之一；为后续 pipeline/driver 的“defer 执行”提供样板。fileciteturn69file0L1-L1

4) **落地第一个压测用例（用例 B）为“设计回归基线”**（负责人：测试/工具工程师 + 核心库工程师，预计 2–3 周）  
输出：在 Windows 主线/CI 中新增一个可重复的 IO flood 场景（notify→pump→dispatch），并用 scheduler 的 stats/trace JSON 做自动验收（posted/dropped/dispatched/filtered、budget_limited）。fileciteturn64file0L1-L1

5) **建立 lint/CI 守卫：禁止非 pump 调用 drain + 禁协议 busy-spin**（负责人：构建与工程效率负责人，预计 1–2 周）  
输出：CI 规则 + 白名单机制（参照 dependency whitelist 的 exception 思路），让“绕过 SSU/契约”变成构建失败而非口头约定。fileciteturn81file0L1-L1 fileciteturn86file0L1-L1