# 主框架全仓审查与收敛 backlog

> status: `archived`
>
> 本文保留一次全仓抽样的具体发现。文件数量、任务状态、路径和优先级只反映归档时快照。

> 范围：整个仓库；重点关注 `Modules/` 主框架代码。  
> 非重点：`Examples/` 默认只作为旁证，不作为本轮主要治理对象。  
> 目标：把“感觉得继续整理”收敛成一份可排期、可分刀、可持续推进的问题清单。

---

## 0. 总体判断

当前仓库的主要问题不是“烂尾式 TODO 太多”，而是：

- **仓库叙事仍处于过渡态**：文档、入口模块、示例代码对同一件事的说法并不完全一致。
- **若干核心模块职责过重**：存在明显的 God Module / God File。
- **某些跨模块模式心智负担偏高**：尤其是 `void* + ops table + trampoline` 的桥接方式已在多个子系统扩散。
- **文档体系信息量大，但状态模型不够显式**：新同学难以快速区分“现行契约”“阶段文档”“草案/任务单”。

反过来说，这个仓库的优点也很明显：

- 第一方代码显式 TODO 很少，说明大多数问题不是“忘了做”，而是**结构收敛还没完成**。
- 文档覆盖率高，说明很多改造都能通过**文档先收口、代码后分刀**的方式推进。

---

## 1. 本轮 review 的结论对象

### 已重点抽样

- `Modules/core/*`
- `Modules/system/*`
- `Modules/io/*`
- `Modules/ui/*`
- `Modules/media/*`
- `docs/*`

### 已确认的总体信号

- `docs/` 共 **164** 篇文档，其中名称含 `draft/plan/v0/tasklist/review` 的有 **36** 篇。
- 第一方代码中显式 TODO 很少，更多是**边界、入口面、职责拆分**问题。
- 若干入口模块 import/export 数量偏大，说明当前模块表面仍然比较“宽”。

---

## 2. P0：建议优先处理的收口项

### T1. 统一 `charm.runtime` 的仓库叙事

**历史现象**

- `docs/architecture/dependency_whitelist.md` 曾说 `charm.runtime` 已移除。
- `Modules/system/charm.runtime.cppm` 曾仍存在可用 facade，且 re-export 了 System / IO / Net / FS / HAL / Shell / Out 等大量模块。
- `docs/architecture_overview.md`、多个 `Examples/*` 曾继续使用或介绍 `charm.runtime`。

**为什么这是 P0**

- 这不是单点代码问题，而是**仓库级认知冲突**。
- 新同学、后续重构者、构建系统维护者会基于不同叙事做出不同判断。
- 这类冲突会直接放大后续一切入口、依赖、迁移工作的沟通成本。

**本轮收口决议**

- `charm.runtime` 已正式退役为 tombstone module，不再作为可用导入入口。
- `Modules/system/charm.runtime.cppm` 不再 re-export 任何模块，正常 runtime source collection 也会排除它。
- `Modules/*`、`Examples/*`、`Draft/*` 均不得再新增 `import charm.foundation;`、`import charm.runtime;` 或 `import charm.domain;`。
- `CHARM_ENABLE_DEPENDENCY_WHITELIST=ON` 会拒绝一方源码中的历史入口导入。
- 示例已迁移到 `charm.core`、`charm.system`、`charm.io` 或更窄的叶子模块入口。
- `charm.foundation` 不删除，但仅保留迁移 facade 语义；当前 first-party 源码不保留 compat import。
- `charm.domain` 不再作为单独模块入口存在；Domain 层统一使用 `charm.media` 与 `charm.ui.*`。
- 原 retirement contract 已删除；现行入口分类见 `docs/architecture/entry_surface_contract.md`。

---

### T2. 给 `docs/README.md` 增加显式“文档状态模型”

**现象**

- `docs/README.md` 的快速开始与专题索引中，现行契约、阶段总结、`v0` 文档、草案、任务单混在一起。
- 新同学很难判断：
  - 什么是“现在应该遵守的”
  - 什么是“历史阶段结论”
  - 什么是“尚未收口的讨论材料”

**为什么这是 P0**

- 文档体系本来是仓库的重要优势，但如果缺少状态标签，优势会反过来变成认知负担。
- 这个问题会影响整个仓库，而不是某个子系统。

**建议方向**

- 在 `docs/README.md` 开头新增文档状态说明，例如：
  - `现行契约`
  - `阶段总结`
  - `任务清单`
  - `草案 / 提案`
- 快速开始区只保留“现行契约”和少量稳定入口。
- `v0 / review / tasklist / draft` 文档下沉到专题索引或项目跟踪区。

---

### T3. 清理已确认失效的文档引用

**已确认的失效引用（现已收口）**

- `docs/architecture/dependency_whitelist.md` 中的 VSF 对照链接曾指向旧路径，现已改正。
- `docs/capability_map.md` 曾直接指向未入库的生成文件，现改由 `docs/generated/README.md` 承接。
- `docs/project/collaboration/《现代 C++ 单片机代码协作认知》.md` 现已接回稳定的 benchmark 与 escape hatch 文档入口。

**为什么这是 P0**

- 这是低成本、高信号的问题。
- 对文档可信度影响很大，但修复成本很低，适合作为“先收一刀”的切入口。

**建议方向**

- 能修正路径的直接修正。
- 尚不存在但确有必要的内容，补“待生成/待补文档”的显式说明，避免静默失效。

---

## 3. P1：建议在当前大阶段内推进的结构治理

### T4. 拆分 UI/Vivid 的 God Module

#### 重点对象

- `Modules/ui/vivid/core/soa_gui.cppm`
- `Modules/ui/vivid/gfx/draw_cmd.cppm`
- `Modules/ui/vivid/core/soa_kernel_payload.cppm`
- `Modules/ui/vivid/core/soa_kernel_class.cppm`
- `Modules/ui/vivid/core/scene.cppm`

#### 现象

- `soa_gui.cppm` 已增长到约 2222 行，仍是最直观的 UI God Module，`record_*` 族职责继续堆积。
- `draw_cmd.cppm` 已增长到约 3432 行，同时承载命令 schema、buffer、batching、executor，规模压力依旧明显。
- `soa_kernel_payload.cppm` 仍有大量按 `PayloadKind` 分发的重复逻辑，但它已经不再是唯一的 kernel 级心智负担源头。
- `soa_kernel_class.cppm` 已增长到约 1559 行，并开始直接承接 semantic state 与 create/destroy 生命周期重置逻辑。
- `scene.cppm` 已增长到约 1680 行，正在演变为 scene/runtime/motion/semantic 的宽 façade。
- `soa_kernel_types.cppm` 虽然体量小于上述几个文件，但 semantic focus/action/request 运行时词汇已进入核心类型层，后续拆分时不能再只盯 payload。
- `focus_scope.cppm` 目前反而相对干净，更像稳定的策略边界，而不是优先拆分对象。

#### 为什么重要

- 这仍然是仓库里最明显的一组“规模 + 职责”双重聚集点，但热点已从单一 `payload` 扩散成 `SoaGui + draw_cmd + kernel class/payload + scene façade` 的组合压力。
- 新近的 focus / semantic / motion 能力已经进入 Vivid 主干运行时，而不是停留在 demo 层；如果不及时收口，后续演进会同时放大 UI、输入、语义和证据面四条链路的心智负担。

#### 建议方向

- `SoaGui` 至少切成：
  - 树遍历 / dispatch
  - style / patch 解析
  - widget recorders
  - perf overlay / debug 辅助
- `draw_cmd` 至少切成：
  - 命令定义
  - 命令缓冲与 batch
  - 命令执行器
- `soa_kernel_payload` 与 `soa_kernel_class` 应联动收口：优先抽公共分发辅助、semantic state 访问与生命周期复位 helper，减少 `PayloadKind` 大 switch 与 class/payload 双向长大。
- `scene` 应优先区分“稳定 public façade”与“runtime / motion / semantic 具体承载层”，避免入口面继续把内部运行时细节整体外翻。
- `focus_scope` 当前保持小而清晰，后续应作为稳定边界保留，避免再次回卷进更大的输入或 scene 实现文件。

#### 第二轮 review 后的可执行分刀

- **T4a. 先收入口面，再拆实现**
  - `charm.ui.vivid` 与 `charm.ui.scene` 继续保持现有 public module 名称稳定。
  - 优先用 module partition / internal module / 同目录窄模块承接拆分，不急着改 public surface 命名。
  - 所有 feature-gated 导出都要与构建配置保持一致；`provider_freetype` 这次 host 阻塞说明入口面已经不能再假设所有 hosted 能力默认存在。

- **T4b. `SoaGui` 第一刀：抽出 style / decoration / recorder families**
  - 当前 `soa_gui.cppm` 头部已经自然形成 `apply_style_*`、`draw_decoration_*`、`record_decorated_box(...)` 一组 helper，适合先抽成 `soa_gui_style_support`。
  - `record_tree(...)` / `record_node(...)` 应保留在主文件，作为 traversal + dispatch 核心。
  - `record_label/button/image/text_box/switch/checkbox/radio/slider/progress` 适合并成基础控件 recorder 组。
  - `record_list/list_item/list_view/table_view/tree_view/scroll_container/text_list/number_list/roller` 适合并成 collection recorder 组。
  - `record_progress_* / record_spinner / record_perf_overlay / record_scrollbar` 更像 feedback / indicator 组，适合作为第三批拆分。

- **T4c. `draw_cmd` 第一刀：先断 schema / buffer / executor**
  - 文件前部的 `CmdType`、`Cmd*` POD、`DrawCmd`、`DrawCmdStats`、`DrawCmdExecStats` 属于稳定 schema，可先独立成 command schema 面。
  - `BlobArena` 与 `DrawCmdBuffer` 已经天然构成第二层，应与 command compaction / readback helper 一起收进 buffer 层。
  - `DrawCmdExecutor` 与其内部 rect/image/text/path batching、group dispatch、tile execute 应作为单独执行层。
  - `mark_bounds(...)`、tile replay / execute_tiles 一类逻辑可以作为更后面的 tile/runtime 层，不必和第一刀绑死。
  - 当前 `switch(cmd.type)` 未覆盖全部枚举值的告警说明 executor 已经开始失去封闭性，拆分后要优先补齐 exhaustive dispatch。

- **T4d. `soa_kernel_class` 第一刀：拆成 storage / semantic / input / widget mutators**
  - `CommonSoA`、`create/destroy`、基础 flag/rect/style setter 属于 storage+lifecycle 核心，应优先稳定。
  - `set_semantic*`、`semantic_snapshot(...)`、`resolve/admit/request semantic_*` 已经形成独立 semantic runtime，可单独出层。
  - `InputActionQueue`、`InputEventQueue`、`InputState`、`FocusScopeFrame`、`input_handle_*` 已经形成 input runtime，应与 storage 主体分离。
  - 大量 `set_list_view_* / set_table_view_* / set_tree_view_* / set_roller_*` 更像 widget payload mutator 集合，适合后置拆分。
  - `focus_scope.cppm` 当前边界清楚，应让 `soa_kernel_class` 依赖它，而不是把 scope policy 再回卷进 class 巨文件。

- **T4e. `scene` 第一刀：拆 façade / builder / layer runtime**
  - 文件开头的大量 `export using` 与 `SceneAccess` 更像 façade 层，应尽量保持薄。
  - `PixelSnapshotPayloadStore`、`CommandSnapshotPayloadStore`、`LayerCaptureResult`、`LayerReplayResult`、`PageLayer` 属于 layer runtime，可形成独立承载层。
  - `LayoutCursor`、`RowBuilder`、`ColumnBuilder`、`CardBuilder`、`TileBuilder` 是 builder/layout helper，不应继续和 runtime snapshot 代码混放。
  - `Scene` 主类应尽量只保留组装、render/dispatch、overlay、layer budget 入口，而不是继续吸纳所有 helper。
  - 当前 `scene.cppm` 同时承接 semantic façade、builder DSL、snapshot runtime、page layer，说明它已经不是“页面 helper”，而是第二个宽入口。

#### 推荐落刀顺序

- 第一步：只做“搬家不改义”的窄拆分，保持 API 与行为不变。
- 第二步：在拆分后的窄文件内补局部 contract / self-check，尤其是 `draw_cmd` exhaustive dispatch 与 `semantic/focus` 运行时边界。
- 第三步：最后再考虑 public façade 是否要进一步收窄，避免一边大拆一边改入口名，放大构建与迁移成本。

---

### T5. 把 `scheduler` 的诊断/格式化职责从调度核心中拆出来

**现象**

- `Modules/system/kernel/scheduler.cppm` 除了调度逻辑，还引入 `out.core / out.format / out.sink`。
- 文件内存在本地格式化 helper 与导出逻辑。
- 还有重复 `#include <string_view>` 的卫生问题。

**为什么重要**

- 当前实现不一定错，但它让调度器承担了太多“诊断输出”职责。
- 长期看，会让 kernel core 更难收敛成一个清晰的最小面。

**建议方向**

- 将 trace dump / JSON / CSV / 文本导出能力拆到专用观察或报告模块。
- `scheduler.cppm` 保留调度、状态、trace 采集，不直接负责复杂格式化。

---

### T6. 收敛 `system_bringup` 的回调桥接模型

**现象**

- `Modules/system/bringup/system_bringup.cppm` 曾经把 `InputBringupDesc`、多个 `BringupMinimal(...)` 重载、若干 `void*` 回调上下文混在一起。
- 第一阶段已移除公开 `InputBringupDesc` / `make_input_desc` 拼装面，`BoardCaps + Host` 构造路径改为在 `BringupMinimal` 内部由 Host 类型 materialize input chain。
- 公开 `BringupMinimal` / `BringupInput` 已移除 `SinkFn + void*` 兼容 overload；raw interop 需要显式写成 `input::RawSinkRef::raw(fn, ctx)`。
- `InputInitChain` / `InputPumpBinding` 已改为传递 `input::RawSinkRef`；裸 callback / ctx 只保留在 `InputPumpTask` 执行窄腰。
- `BringupMinimal` 已移除公开 `ReactorPumpTask& + PostFn + void* post_ctx` legacy 构造入口；bringup 公开入口统一走 `BoardCaps + Host`。

**为什么重要**

- bringup 是平台入口，越靠近入口，越应该降低隐式约定。
- 这里的复杂度会直接传递给后续板级适配和构建装配。

**建议方向**

- 后续继续把 sink 能力收敛成更明确的 provider ref。
- 底层 `InputPumpTask` 的 callback / ctx 暂视为 scheduler / EDA 窄腰，不在 bringup 层重新包装成公开 descriptor。

---

### T7. 把 Net API 自检脚手架从生产模块中挪走

**现象**

- `Modules/io/net/net.api.cppm` 存在 `#ifndef NDEBUG` 的 `ApiDummyProvider` 和 `net_api_self_check()`。
- 目前仓库内没有其他地方引用该自检入口。

**为什么重要**

- 这类自检资产本身有价值，但不适合继续挂在正式 API surface 上。
- 会模糊“生产导出面”和“调试/自检资产”的边界。

**建议方向**

- 移入：
  - `net.*.self_check.cppm`
  - 或测试/内部验证专用模块
- 正式 API 模块只保留业务导出。

---

### T8. 收敛 `fs_fatfs` 的“真实现 + 全局桥接 + stub” 混合职责

**现象**

- `Modules/io/fs/fs_fatfs.cppm` 同时承担：
  - FatFs mount/file 适配
  - `diskio` 风格全局桥接状态
  - `CHARM_USE_FATFS` 关闭时的 nosys stub

**为什么重要**

- 这会让一个模块同时拥有“真实运行态职责”和“配置兜底职责”。
- 后续若继续增加 cache、multi-drive、block 适配策略，会更难维护。

**建议方向**

- 至少拆成：
  - `fs_fatfs.core`：真实 FatFs 适配
  - `fs_fatfs.diskio_bridge`：`disk_*` 全局桥接
  - `fs_fatfs.stub`：禁用场景兜底

---

## 4. P2：中期治理项

### T9. 审视 `void* + ops table + trampoline` 作为跨子系统默认模式

**现象**

- Net、Bringup、部分 IO 桥接大量使用：
  - `void* ctx`
  - ops table
  - lambda trampoline

**风险**

- 可移植、可静态分配，但类型信息和生命周期约束更多依赖人为约定。
- 一旦跨更多子系统扩散，后续回看代码会越来越像“我知道它能跑，但我需要重新读一遍才能确认为什么能跑”。

**建议方向**

- 不必一刀切禁止。
- 先建立一个轻规则：
  - 默认优先 typed provider/ref
  - 只有跨 ABI、跨 C 边界、跨异构后端时再退回 `void* + ops`

---

### T10. 收敛入口聚合面的规模

#### 重点对象

- `Modules/core/charm.core.cppm`
- `Modules/system/charm.system.cppm`
- `Modules/ui/vivid/charm.ui.vivid.cppm`

#### 现象

- 当前若干入口面 import/export 数量偏大。
- `charm.core.cppm` 曾存在重复导出 `service_fifo` 的卫生问题，当前已清理。
- `charm.runtime.cppm` 已退役为 tombstone，不再纳入入口聚合面规模治理。
- 入口面分类已落到 `docs/architecture/entry_surface_contract.md`；历史入口已由 opt-in 白名单检查封住。
- 原 stable aggregate contract 已删除；仍然偏宽的长期入口由
  `docs/architecture/entry_surface_contract.md` 与实际聚合模块共同审查。

#### 建议方向

- 重新区分：
  - 稳定入口
  - 兼容入口
  - tombstone / retired entry
  - 产品/场景聚合入口
- 不要求所有入口都变小，但要让“为什么这里可以大”变得可解释。
- 下一步重点不再是历史入口回流，而是解释和压缩仍然偏宽的稳定聚合入口。
- 当前已完成“解释边界”，后续再进入具体拆分。

---

### T11. 拆分 `audio_player` 的多重职责

**现象**

- `Modules/media/audio/audio_player.cppm` 同时包含：
  - 宏配置开关
  - 本地 utility 类型
  - fallback sink
  - refill / stress / stats / 主状态机

**为什么值得关注**

- 当前它已经像一个“局部子系统单文件实现”。
- 未来加格式、sink、诊断时容易继续发胖。

**建议方向**

- 优先拆出：
  - `player_types / local_utils`
  - `player_stats / debug`
  - `player_core`

---

### T12. 拆分 `usb.class_msc` 的协议/追踪/状态机职责

**现象**

- `Modules/io/usb/class/usb.msc.cppm` 当前把 BOT、SCSI、sense、trace、storage bridge 混在同一实现中。

**建议方向**

- 优先抽出 trace/sense 辅助结构。
- 再考虑将 BOT 状态机与存储桥接分离。

---

## 5. 低成本卫生项（适合穿插完成）

这些问题不大，但很适合顺手收口：

- `Modules/core/charm.core.cppm` 重复导出 `service_fifo`（已清理）
- `Modules/system/kernel/scheduler.cppm` 重复 `#include <string_view>`
- 已确认失效文档路径修复
- 为 `docs/README.md` 增加更明确的“现行/草案/任务单”导航说明

---

## 6. 建议推进顺序

建议按下面的顺序推进，而不是同时开很多大坑：

1. **先收仓库叙事**
   - `charm.runtime`（已完成，保留 tombstone）
   - 文档状态模型
   - 失效引用
2. **再切一组最痛的 God Module**
   - UI/Vivid 三件套优先
3. **然后处理跨子系统模式**
   - `void* + ops`
   - bringup / net / fs 的 typed facade 收敛
4. **最后统一入口面策略**
   - 历史入口分类已完成，继续治理仍然偏宽的稳定聚合入口

---

## 7. 推荐的任务看板（第一版）

| 任务 | 优先级 | 范围 | 性质 | 状态 |
|---|---|---|---|---|
| T1 统一 `charm.runtime` 仓库叙事 | P0 | Docs + Modules + Examples | 收口 | DONE |
| T2 给文档索引增加状态模型 | P0 | Docs | 收口 | DONE |
| T3 清理失效文档引用 | P0 | Docs | 快速修复 | DONE |
| T4 拆分 UI/Vivid God Module | P1 | UI | 结构治理 | TODO |
| T5 拆分 scheduler 观察职责 | P1 | System | 结构治理 | TODO |
| T6 收敛 bringup 回调桥接 | P1 | System | 边界治理 | DONE |
| T7 挪走 Net API 自检脚手架 | P1 | Net | 边界治理 | DONE |
| T8 拆分 `fs_fatfs` 复合职责 | P1 | FS | 结构治理 | TODO |
| T9 审视 `void* + ops` 默认扩散 | P2 | Cross-cutting | 模式治理 | TODO |
| T10 收敛入口聚合面规模 | P2 | Core/System/UI | 架构治理 | PARTIAL |
| T11 拆分 `audio_player` | P2 | Media | 结构治理 | TODO |
| T12 拆分 `usb.class_msc` | P2 | USB | 结构治理 | TODO |

---

## 8. 本文档的使用方式

- 它不是一次性审判书，而是**后续几轮治理的排期底稿**。
- 当某条任务开始推进时，可以基于本文档再派生更窄的子任务单。
- 当某条任务完成后，优先更新本文档状态，而不是重新发明一份新的“大而全 review”。
