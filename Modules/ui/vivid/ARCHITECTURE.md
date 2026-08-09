# Charm Vivid Source Architecture

## 文档状态

- `status`: `supporting`
- `scope`: Vivid source 分层、渲染、静态内存、catalog、layout/input/style 边界
- `authority`: `Modules/ui/vivid` source、`vivid.cmake` 与生成证据

本文不维护 roadmap、widget 全量清单、demo 完成度或 C++ API 镜像。产品语义与 evidence 入口见
[`docs/ui/README.md`](../../../docs/ui/README.md)。

## 分层与公共入口

| 层 | 责任 | 依赖边界 |
|---|---|---|
| `core` | Scene、SoA kernel、layout、input、style、layer/motion runtime、配置 | 可依赖基础 gfx；不依赖产品或 backend |
| `gfx` | geometry、pixel/color、canvas、DrawCmd record/execute、host evidence tools | 不拥有 widget/product 状态 |
| `widgets` | 控件行为和薄 factory/payload 适配 | 复用 core/gfx/font，不复制基础设施 |
| `font` | 固定字体数据、typography 与 package/provider runtime | provider 生命周期和 IO 不进入热路径 |

产品入口与内部入口必须区分：

- `charm.ui.vivid`：产品根入口，只暴露已批准的 core style/config、基础 gfx 与 Scene；
- `charm.ui.scene.motion_runtime`：受 PRODUCT closure 管控的 layer/motion 扩展；
- `charm.ui.vivid.font_runtime`：字体 package/provider 聚合，不导出 FreeType provider；
- `charm.ui.vivid.perf_overlay_runtime`：固定容量诊断通道与统计快照，不携带 object widget；
- `charm.gfx.host_tools`：host snapshot/DrawCmd 证据工具，不进入默认 PRODUCT profile；
- SoA kernel、DrawCmd partitions 与 `charm.ui.vivid_internal`：实现入口，产品不得直接 import。

PerfOverlay runtime 是单 UI execution domain 的应用级诊断通道，不属于任一 Scene；其固定槽、profile
counter 与 DrawCmd policy 常驻字节必须进入 Vivid global static-memory profile。debug channel 名称接受
1..23 字节；空名、超长名或槽位耗尽返回 `debug_line_count` sentinel，不能截断后制造别名或重复耗槽。
清空 channel registry 同时清空对应文本，避免后续复用槽位时泄漏旧诊断内容。

完整 import 规则见
[`vivid_import_boundary_contract.md`](../../../docs/ui/vivid_import_boundary_contract.md)。

## Render Model

Vivid 采用固定容量 record/execute 路径：

```text
widget/scene state
    -> layout + invalidation
    -> DrawCmdBuffer record
    -> optional compaction
    -> FullFrame or Tile/PFB execute
    -> backend flush/present
```

稳定不变量：

- widget 只记录绘制意图，不直接绑定平台 canvas/driver；
- 每个 Scene 只有一套 live DrawCmd runtime、compaction workspace 和 executor；
- Scene、SoaGui 与 SoaKernel 是具有稳定 identity 的 runtime owner，禁止复制或移动；多个场景必须显式
  构造独立 Scene，不能复制 runtime state；
- command/text/blob/workspace 容量固定，耗尽必须产生显式 sticky evidence，不静默截断；
- FullFrame 与 Tile/PFB 消费同一 record 语义；不同 backend 结果需要一致性证据；
- command snapshot 只复制已使用的 command/text/blob payload，不复制 live buffer 元数据或临时 workspace；
- command snapshot replay 支持 identity 与整数平移；平移叠加到 Canvas 既有 origin，执行后必须恢复，target
  clip 逆变换到 source 坐标，命令内嵌 clip 只能继续收窄。非 255 opacity 仍必须在执行命令前返回
  `UnsupportedTransform`。`PageTransitionRunner` 只在 recipe 全程保持 255 opacity 时采用单 source command
  snapshot，并以 live destination 合成；prepare 导致 epoch 变化或实际 command 工作量超过 budget 时，必须在
  首帧前释放 snapshot，并以 `StaticCut` 及对应 fallback reason 写入 trace/ledger；
- snapshot handle slot 与 payload slot 一一对应；`HYBRID` 中 command 与 pixel payload 共享同一块按较大者
  定额的 slot storage，`COMMAND` / `PIXEL` 只保留对应 kind 的 slot，`NONE` 不保留 payload capacity；
  同一逻辑槽不能让两种 kind 同时占用，禁止恢复两套各自乘 `layer_cache_slots` 的常驻池；
- snapshot storage mode 属于 product profile，决定编译进产品的能力和 `Scene` 布局；slot 数、尺寸和 pixel
  format 属于 target envelope。未编译 kind 的 capture 必须在 reserve 前返回 `UnsupportedKind`；
- `layer_cache_slots=0` 是合法的无快照 target envelope：Scene 保留稳定 API，但 capture 必须返回无槽，admission
  必须降级为 `StaticCut` 或 `Reject`，静态内存画像中的 command/pixel payload capacity 必须同时为零；
- compaction 在 record 完成后执行，不能改变命令可观察顺序或 artifact 语义；
- compaction workspace 复用的 `DrawCmd` 必须通过类型默认值赋值重置，禁止以 `memset` 假设非 trivial 对象
  的默认语义等于全零位模式；该 reset 必须保持 `noexcept` 且不引入局部大对象；
- rect/line/path/glyph/image batch scratch 生命周期互斥，只能共享一块按最大 item 对齐的 fixed byte storage；
  item 必须 trivially copyable 且具有唯一 object representation，通过 `memcpy` 写入最终 raw blob 所需字节；
  二进制可见 padding 必须改为显式 reserved，禁止恢复五张并存数组或未受对象生命周期约束的 reinterpret 写入；
- executor 对相邻 rect/image/line/path/text run 单遍读取并立即执行；禁止恢复“先收集到 64 项 typed array、
  再逐项执行”的常驻中间表。clip stack 与 tile-hit table 在 Tile 执行期间真实重叠，仍由 executor 独立持有；
- command arena 只按 canonical `CmdHeader + 最大 payload` stride 乘算，不常驻随机索引 offset table；
  executor、compaction、snapshot 与 evidence 均按 byte cursor 遍历。load/replay 在一次扫描中验证并解码每条
  record、重建 command count，并拒绝未知 type、payload 不足或超过 canonical stride 的扩展 record；失败后
  stream 必须保持为空，executor 遇到运行期损坏必须有界退出，不能让外部字节绕过容量证明或锁死渲染；
- tile 命中索引容量不足时必须回退到正确但更慢的扫描路径；
- 产品 evidence 只消费 Scene stats 和 artifact 摘要，不依赖 CmdHeader/payload 私有布局。

`Scene` 的 builder/layer support 是附属实现层，render detail 是 private partition；它们不建立新的产品或
evidence surface。DrawCmd 与 state-to-artifact evidence 分别见：

- [`vivid_draw_cmd_evidence_boundary_v0.md`](../../../docs/ui/vivid_draw_cmd_evidence_boundary_v0.md)；
- [`vivid_render_evidence_chain_v0.md`](../../../docs/ui/vivid_render_evidence_chain_v0.md)。

## Workspace 与静态内存

SoA layout、render、hit-test、focus 与 semantic traversal 共享 `SoaKernel` 唯一拥有的一套固定 frame
workspace；容量与 `soa_max_nodes` 相同。正常 runtime frame 固定为 52B；仅开启 Draw Detail evidence 时增加
`draw_scope_id` 并固定为 56B，PRODUCT 不得为关闭的 evidence 能力常驻该字段。各阶段通过 move-only lease
串行借用，不能同时保留两套 typed stack。焦点 next/previous 使用单趟遍历维护首尾和相邻候选，不再为所有
节点常驻第二张 candidate 表。

Draw Detail evidence 同时为每个 SoA node 增加 2B `draw_scope`；配置期 node upper 必须随 feature 从 209B
切换到 211B。frame 与 node 两项都必须进入同一 target envelope，不能只计算其中一项。

同一 Scene 上的并发或重入会拒绝后进入的 traversal，并产生 sticky phase-conflict 与 workspace-overflow
evidence；lease 释放后 workspace 必须可再次使用。compaction、DrawCmd executor 与 raster scratch 仍使用各自
职责明确的固定 workspace，不与 traversal frame 做无生命周期证明的内存覆盖。

PRODUCT/MCU profile 必须声明：

- SoA node/payload、TextArena、semantic/StylePatch 稀疏槽、Style、DrawCmd 和 snapshot storage mode；
- layer cache 槽数、尺寸及 pixel format；
- Scene 数量、常驻 RAM 上限、最小 headroom 与 stack frame 上限；
- screen/pixel format 和 backend envelope；
- overflow、workspace exhaustion 和 payload ownership 的失败行为。

`-fstack-usage` 只能约束单函数 frame，不能证明调用链峰值。产品任务栈仍需入口分析或运行时
high-water evidence。静态内存准入见
[`vivid_static_memory_admission.md`](../../../docs/ui/vivid_static_memory_admission.md)。

## Payload、Catalog 与 PRODUCT Profile

Widget node 不保存可逃逸的 payload handle。节点未使用时，16 位 storage slot 表示 node free-list link；
节点活动时，同一字段表示 payload slot。每类 payload 使用固定容量 pool，并常驻记录当前 owner node index；
旧 owner 对已复用槽的读取或释放必须被拒绝。公开 `WidgetHandle` 的 generation 继续独立保护 node identity，
不能用 payload slot 代替公开 handle。

SoA 子树以 `first_child` / `next_sibling` 链作为直接子节点集合与顺序的唯一真相，不常驻第二份逐节点
`child_count` 表。首子节点的内部 `prev_sibling` 槽编码同一条 intrusive 链的 tail，`last_child()` 与
`prev_sibling()` helper 隔离该布局，因此 append、detach 与逆序 traversal 继续保持 O(1)，同时不常驻独立的
`last_child` 表。`child_count()` 仅在调用时做 `soa_max_nodes` 有界遍历；debug 构建必须对越界索引或 sibling
环断言。该查询不属于 layout、render、input 或 semantic 热路径；未来若出现热路径计数需求，必须以真实消费
证据重新评估索引结构，不能恢复无条件的 2B/node 缓存。

Semantic role、id、label 与 action mask 只由显式 semantic entry 使用，不随每个 node 常驻。node 保存强类型
8 位槽索引，`SoaKernel` 按 profile 的 `SEMANTIC_SLOT_CAP` 拥有固定容量 semantic pool，容量不得超过 255。
set 覆盖已有槽；clear 与
node destroy 归还槽；action-only 更新不能隐式创建 entry。池满时保留已有 entry、拒绝新 entry，并产生 sticky
overflow、allocation-fail、live/peak evidence。`WidgetKind`、Scene semantic API 与 snapshot 值语义不随存储迁移改变。

`cmake/widget_catalog.cmake` 是 WidgetKind、module、factory、payload、style/default/input behavior 的
单一构建入口。稳定 kind ID 由 ABI fixture 固定；PRODUCT profile 只裁剪能力和容量，不生成另一套 enum。
每个 catalog kind 必须显式声明 Scene runtime 是否已支持；object widget 存在或 module 可进入 closure 都不等于
Scene 能创建、派发并记录该 kind。PRODUCT profile 选择未支持 kind 必须在配置期失败。
`WIDGET_KINDS` 只声明 Scene/SoA 能力并驱动 payload 容量；`OBJECT_WIDGET_KINDS` 只按需引入 catalog 的
`OBJECT_MODULE`。object-only 控件不得因此启用同名 SoA kind 或预留 payload，Scene kind 也不得自动携带
object module。两套表面可以使用同一稳定 `WidgetKind` 身份，但其实现准入和 RAM 成本必须独立证明。

Product Profile Compiler 是 Vivid 工具，不是 Charm Core 或产品 C++ API：

- C++ `module/import/export import` 是依赖边唯一来源；
- CMake policy 只标记 product root、internal、host-only 和硬件 envelope；
- profile 固定产品 root、active Scene WidgetKind、按需 object widget module、snapshot storage mode 和工作集；
  target envelope 固定设备资源与 snapshot slot 几何；
- 一个 target 只能选择一个不可变 profile/envelope；漂移、未知 root、cycle、internal root、catalog/pool
  不一致必须在 configure 阶段失败；
- fingerprint 和 generated evidence 证明规范化输入，不证明运行行为或视觉正确。

## Layout 与 Invalidation

基础 layout 支持 Anchor、Flex、Flow、Grid、Constraint 和显式注册的 Custom engine。container 负责 child
layout 与 clip/viewport；滚动、虚拟列表等行为不能绕过统一 layout/invalidation 入口。

Object-level widget 不再暴露通用 layout spec、anchor、cache policy 或 dirty hint。这些状态原本只服务于已删除的
legacy Gui/layout 执行器；继续保留会让每个对象支付 RAM，并让调用方误以为写入能够影响产品布局或重绘。
Object widget 只保留显式 `Rect`、视觉状态和控件自身的具体方法；`ObjectBase` 不保存 parent graph，也不提供
draw/event/geometry 手写动态派发。固定容量容器拥有 child handle，通用 resolver 只借用基类几何与状态面；
产品 layout 与 invalidation truth 属于 SoA Scene/kernel。未来若增加 object-level layout 或动态派发扩展，必须
同时拥有明确执行器、固定预算和 evidence，不能只向 `ObjectBase` 增加被动配置字段或函数表。

SoA node 的当前 `Rect` 同时是 layout、paint culling 与 hit-test 的几何真源，不保存第二份逐节点
`paint_bounds`。历史副本只在第一次 `set_rect()` 时写入，重布局后会陈旧且仓库没有 override 消费者，因而已
硬删除。未来若确有阴影、滤镜等越界绘制需求，必须以按需 decoration/effect bounds 和独立容量 evidence 引入，
不能恢复所有节点常驻的备用矩形。

SoA node 的 layout kind 与 label 水平/垂直对齐保持原强类型 API，但内部共享一个显式 mask 编码的 1B state。
当前 layout 2 值、双轴对齐各 3 值均在源码 `static_assert` 的 2-bit 上界内；调用点不得依赖 bit layout。layout
更新不能覆盖对齐，对齐更新不能覆盖 layout，node create/destroy 必须恢复 `None/Left/Center` 默认值。

状态影响由 source 中的 `layout_state_influence_mask(kind)` 决定：

| 变化 | 默认影响 |
|---|---|
| enabled/hovered/pressed/focused | paint；只有 source mask 明确允许时才能触发 layout |
| text/content 或 text metrics | layout + paint |
| rect、layout kind、row height、影响几何的 range/spec | layout + paint |
| color/paint-only style | paint，不得伪造 layout invalidation |

状态写入时根据 delta 与 mask 选择 layout/paint dirty；layout pass 也只能读取 mask 允许的状态位。文档表
不是 widget 全量真相，新增 kind 时必须同步 source policy 和回归矩阵。

## Input 与 Focus

SoA kernel 统一处理 hit-test、capture、drag/cancel 和 focus。dispatch 先记录 action，再在受控提交阶段修改
状态，避免 widget 分散写入 hover/pressed/focused truth。

Object-level widget 不在 `ObjectBase` 常驻通用交互表。需要 gesture/interaction strategy 的控件显式拥有并
直接派发对应 strategy；自定义控件需要组合多个 strategy 时，可以自行持有固定容量 `InteractionList`。
未使用交互扩展的 object widget 不支付其 RAM 成本。

`ScrollContainer` 不为每个 child 常驻逐项布局基准数组。`sync_child_bases()` 在 layout 完成后记录容器
原点与当前 scroll，`apply_scroll()` 只对已解析的 child 应用统一的整数平移增量；容器移动和滚动变化不会
复制或重建 child 布局表。两个操作都在修改前完整解析 child，resolver 缺失时返回 `false` 且不部分移动；
调用方应在 child layout 或集合变化后重新同步，resolver 必须在一次调用期间保持稳定。

`ScrollContainer` 的背景、边框、focus ring 与 scrollbar 统一从 Theme/StyleSheet 解析，不保存 instance-local
`Style`、nine-slice skin 或调试型拖动文本。逐实例差异使用 style class 或 local patch；图片化外观应由显式
装饰控件组合，而不是让每个基础滚动容器常驻未使用的 image/font 状态。

高频 `Button` 同样不保存 instance-local `Style` 或 nine-slice skin；背景、边框、字体和 focus ring 统一从
Theme/StyleSheet 解析。object `Button` 是纯文本 command 控件，不为无人使用的 image 能力预付 `ImageView`；
SoA `WidgetKind::IconButton` 是由 Button payload、image registry 与 record pipeline 实现的独立 kind，不等同于
object `Button` 的类布局。逐实例视觉差异使用 style class 或 local patch，图片化背景通过显式装饰组合实现。

`ObjectBase` 同样不常驻子节点数组。`WidgetBase<Derived, ChildCapacity>` 的默认容量为零；固定 inline 模式
只供明确选择编译期容量的自定义控件。内置 List、FoldablePanel、ScrollContainer、RadioGroup 使用
`std::dynamic_extent` 外部模式，由调用方通过 `attach_child_storage(std::span<WidgetHandle>)` 提供实际容量；
未 attach 时容量为零且 add/insert 返回 `false`。storage 必须独占并覆盖容器使用期，detach 会清空 active
handle，容器析构也会清空仍 active 的 handle。容量耗尽显式返回 `false`，不分配动态内存；ScrollContainer
的 resolver 在一次调用内必须稳定。

`RadioGroup` 是无视觉的 object 互斥策略，不为历史 16 项上限常驻句柄表。调用方按实际 radio 数量提供独占
storage；未 attach 或容量耗尽时 `add()` 返回 `false`，detach 与析构遵循相同的 active handle 清理规则。

`FoldablePanel` 的标题和正文同样按需借用调用方提供的 NUL 结尾文本，不在每个实例中预留固定文本缓冲。
文本 owner 必须覆盖控件的完整绘制周期；传入 `nullptr` 等价于空字符串。需要独立文本所有权时，由上层状态或
专用文本存储承担，不能把最大内容容量重新乘到每个折叠面板实例上。

`Label` 及组合它的 object widget 借用调用方文本，并在 setter 时记录最多 64 字节的 admitted range；只读
`TextBox` 使用相同所有权模型，admitted range 为 256 字节。测量与绘制都使用显式长度，不重新做无界 NUL
扫描。owner 必须覆盖控件使用期；原地修改文本后必须再次调用 setter 刷新长度和几何。需要更长或自有文本时，
应使用 SoA TextArena 或显式的上层文本存储，而不是恢复逐实例缓冲。可编辑 `TextInput`/`TextArea` 必须保存
编辑状态及内容，不能套用只读 view 的借用契约。

同一规则覆盖其余只读 object 文本控件：`CodeBlock`/`RichText` 记录最多 256 字节。`Timeline`/`Roller` 的每项
记录最多 32 字节，`Stepper` 的每项记录最多 16 字节，但三者的 pointer/length 表由调用方以两个等长 span
按实际容量提供；控件只保存 data pointer、8 位容量和 active count，不再常驻最大槽表。attach 对长度不等或
超过 Roller 16、Stepper 8、Timeline 16 项的配置整次返回 `false`；未 attach 时 setter/add 返回 `false`，
空 Roller 不消费输入事件；
detach 与析构清空 active storage，析构不发送 state 通知。文本与两个 storage span 都必须覆盖控件使用期；
原地修改 admitted range 内的字节会被后续绘制观察到，若指针或文本长度变化则必须重新调用对应 setter。
编辑型控件继续拥有自己的内容缓冲，不能借此规则外置编辑状态。

Object `Dropdown` 不注入默认展示文本，也不常驻 8 槽 option 指针表。调用方按实际 option 数量提供独占
`std::span<const char*>`；storage 与 label owner 都必须覆盖控件使用期，未 attach 或容量耗尽时 `add_option()`
返回 `false`，detach 与析构清空 active pointer，`nullptr` label 归一化为空字符串。显式 detach/reattach 将
selected truth 复位为 0 并通知 state observer；析构不发通知。当前 SoA
`WidgetKind::Dropdown` 仍是 catalog 中的稳定 kind 占位，record pipeline 明确报告 unsupported，不能把 object
ownership 收敛误报为 SoA Dropdown 已实现。

Object `SegmentedControl` 与 `ToggleGroup` 同样不常驻 8 槽 item 表。SegmentedControl 以最多 8 项的
`std::span<const char* const>` 借用只读 label 列表；列表缩短会通过 selected state observer 报告 truth clamp，
但不触发 command callback。ToggleGroup 以最多 8 项的 `std::span<ToggleGroup::Item>` 直接读写调用方拥有的
label/checked 模型，单选和点击结果因此无需复制回上层。两个 span 及其文本都必须覆盖控件使用期，超限配置整次
返回 `false` 且不部分替换；读写与 draw/input 必须位于同一串行 UI execution domain。SoA 同名 kind 继续使用
独立 payload/input 路径，不借用 object span。

`DropdownPopup` 作为 object/SoA 混合 helper 同样借用调用方提供的 option 指针数组和文本；二者都必须覆盖
popup 的完整配置与绘制周期。修改文本内容或数组项会被后续 list source 读取观察到，`nullptr` 项继续归一化为
空字符串；helper 不再为 16 个选项常驻字符串副本或二次指针表。committed selection 仍由 helper 的
`service::state` 持有并通过 `observe_selected()` 同步观察；confirm edge 只保存一个带 committed index 的 typed
delegate，即使重复确认当前 selection 也会调用。需要多播时由调用方显式转发，helper 不常驻第二张 signal 表或
无参 legacy callback。DropdownPopup 关联 SoA handles、state connections 与外部 callback target，禁止复制和移动。

`MenuTree` 的 highlight truth 属于调用方提供的 selection model；confirm edge 只保存一个带完整
`menu_item_ref` 参数的 `util::delegate`。需要同步多播时由调用方显式拥有 `service::signal` 并从该 callback 转发，
helper 本体不常驻固定槽表，也不并存无参 legacy callback。MenuTree 关联 SoA handles 与外部 callback target，禁止
复制和移动。

Object `TabView` 不再常驻 6 组标题与 page handle。调用方按实际 tab 数量提供独占
`std::span<TabView::Tab>`；storage 必须覆盖控件使用期，未 attach 或容量耗尽时 `add_tab()` 返回 `false`，
detach 与析构清空 active entry。标题继续借用调用方文本，page 只保存 handle；resolver 无论在 tab 前还是 tab 后
绑定，都必须立即把 active page 设为 visible、其余 page 设为 hidden。SoA `WidgetKind::TabView` 使用独立的
segmented payload/input 路径，不与 object storage、标题指针或 resolver 生命周期混用。

Object `TableView` 不再为每个实例常驻 16 项列宽表。需要逐列覆盖宽度时，调用方按实际列数提供独占
`std::span<int>`；未 attach 或越过 storage 容量时 `set_column_width()` 返回 `false`，detach 与析构清空 active
width。只读 `ColumnWidthFn` 路径不需要 storage。object 绘制、滚动边界与 hit-test 最多处理 16 列，回调返回更大
列数也会被裁剪；storage 与 callback context 必须覆盖控件使用期。SoA `WidgetKind::TableView` 使用独立 payload、
record 与 input 路径，不借用 object storage 或 callback。

跨帧保存的 UI callback 使用 `util::delegate` 并由 owner 保证 target 生命周期。历史名称 `Callback` 只是
`util::delegate<>` 的单轨别名，不得重新增加独立的 `fn + void*` fallback 或第二份 callback 存储。
Button、ListItem、MenuItem 这类单一 command edge 每个实例只保存一个 delegate；需要同步多播时，由调用方
显式拥有 `service::signal` 并从该 delegate 转发，不能让所有控件预付固定槽表。truth/value widget 是否保留
`service::state` 由其可读状态与真实 observer 消费证据单独裁决，不能与 command edge 混为一谈。
会由自身输入改变 truth、且已有直接 observer 消费证据的 object widget 只保留一个同步观察槽；第二个直接连接
必须失败，释放后槽位可复用。需要一对多联动时，controller 从这个边界显式转发到 caller-owned
`service::signal`，不能把多播容量重新乘到每个控件实例上。只由 setter/range 配置驱动的展示控件直接保存标量，
不为 API 对称暴露 `observe_value()`；上层若需要观察，应观察其状态真源，而不是展示副本。
`std::function_ref` 只适合未来同步、非逃逸的函数参数，不得存入 widget、strategy、Scene 或 payload。绑定 owner 自身的控件必须
禁止复制/移动，或显式实现 callback 重绑；保存成员 strategy 地址的 `InteractionList` 同样不可复制和移动。
strategy 绑定外部长生命周期 target 时仍可按值复制，不能把该合法场景误收紧为全局禁用 delegate 复制。
返回型 data-source callback 可以在明确的固定成本 type-erasure 边界使用函数指针与 `void*`，但每个函数必须
只使用同一 setter 配对保存的 context；draw、row-height、row-flags、selection 等 callback 不得隐式借用
另一个 data source 的 context。owner 必须覆盖 callback 的完整保存期。

ListView/TreeView 的虚拟 item pool 同样按需付费。控件本体只保存 non-owning workspace 指针；未 attach
`ItemPoolWorkspace` 时，`DrawInfo::slot` 保持 `-1`，不遍历 cache，也不常驻槽表。workspace 同一时刻只能
attach 到一个同类型控件，控件与 workspace 均不可复制或移动；任一方销毁或显式 detach 时先回收 live slot
并解除绑定。workspace 内保存的 pool/cache callback target 必须覆盖其配置与回收周期。

ConsoleBox 的 object surface 只负责视图行为，不在每个控件实例中常驻文本历史。调用方按所需行数拥有
`ConsoleBox::Buffer::Line` 数组和 `Buffer`，再显式 attach；未 attach 时 append 为无副作用操作。Buffer 可以
被多个只读视图观察，但其写入与视图 draw 必须位于同一串行 UI execution domain，且 Buffer/行存储必须覆盖
所有 attach view 的使用期。SoA ConsoleBox 继续使用 kernel payload/TextArena，不与 object Buffer 混用。

Chart、Histogram、HistogramView 与 WaveformView 是 caller-owned data view，不复制样本数组。直接数据使用
有界 `std::span<const int>`；需要惰性来源时，Chart/Histogram 的 callback 每次 draw 只返回一次 span，随后整帧
使用同一 view。span、callback context 及其底层数据必须覆盖 draw 周期，且更新与 draw 位于同一串行 UI
execution domain。各控件仍保留原最大点数作为单帧工作上限；SoA 同名 kind 的 payload/record 语义不受影响。

SpectrumView 的 values 同样是最多 32 项的 caller-owned span；跨帧 peak history 则由可选、独占 attach 的
`PeakWorkspace` 保存。未 attach 时控件仍绘制当前值，但不支付 peak 数组 RAM，也不保留历史峰值。每次 draw
将 peak 更新为 `max(current, peak - decay)`，并清零超出当前 values 长度的槽位；workspace 及其 float 存储
必须覆盖 attach 周期。关闭 float widgets 的 profile 不执行数值推进或频谱绘制。

DynamicNebula 的粒子状态同样不常驻控件本体。调用方以 angle/offset 两个 `std::span<float>` 构造独占
`ParticleWorkspace`，有效容量取两个 span、请求粒子数与 64 上限的最小值，因此 RAM 成本为 8B/粒子而不是
固定 64 槽。未 attach 时不推进或绘制粒子；workspace 同一时刻只能绑定一个控件，显式 detach 或任一方析构
都会解除双向引用。workspace 与底层 float 数组必须覆盖 attach 周期，关闭 float widgets 时仍不执行粒子绘制。

控件实现语义行为，不拥有全局 focus/navigation policy。focus truth、scope、semantic request 与 visual
focus artifact 的边界从 [`vivid_focus_evidence_boundary_v0.md`](../../../docs/ui/vivid_focus_evidence_boundary_v0.md)
进入。

## Text 与 Font

- TextArena/TextId 保存文本，不把临时指针存入 payload；溢出必须可观察并按契约 fallback；
- UTF-8 decode、measure、wrap、truncate 和 glyph fallback 集中在 text/font runtime；
- 固定字体数据由 builder 生成，生成物不反向定义 typography 语义；
- VFS/package provider 的 Font 指针在 provider 生命周期内保持稳定；
- loader 不得在 render 热路径执行阻塞 IO；host FreeType 是 provider，不进入 Vivid 产品入口。

字体尺寸、metrics 与生成规则见 [`font/font-metrics.md`](font/font-metrics.md) 和对应 source/config。

## Theme 与 Style

theme token 经 ResolvedTheme/StyleSheet 预编译为可索引 style；热路径不重复派生 role 或搬运大对象。
规则优先级必须确定，metrics pool 与颜色/state 表保持固定容量。

`StylePatch` 本身保持 bit-packed、trivially copyable，并由源码 ABI 上限阻止 presence flag 退回逐字节布尔
存储。SoA node 不常驻完整 patch，只保存一个强类型 8 位稀疏槽索引；`SoaKernel` 按 profile 的
`STYLE_PATCH_SLOT_CAP` 拥有固定容量 patch pool，容量不得超过 255。首次 set 分配槽，覆盖已有 patch 不增加
live count，clear 和
node destroy 归还槽。容量耗尽时保留所有既有 patch、拒绝本次写入，并产生 sticky overflow 与 allocation-fail
evidence；PRODUCT 必须显式声明槽容量，且不得超过 node capacity。字段打包与稀疏化都不改变 patch 优先级、
adjust/override 语义或公共 `Style`/`StylePatch` API；真实 Scene/StyleSheet 收益由 GCC 目标 ABI evidence 记录。

SoA 与产品配置以 `StyleSheet` 的 `WidgetKind` base style 为真源；`set_base_style` 与 `patch_base_style` 自行标记
compiled table dirty，调用方不得依赖额外 notify 才让写入生效。`Theme::get<T>/patch<T>` 的 type slot 只服务
object widget 兼容面，不能作为 SoA style 写入或要求产品仅为类型标签 import object widget module。

style state mask 只包含该 WidgetKind 真正消费的状态维度。Focused 默认属于 focus artifact，不自动进入
普通 style mask。展示、press-only、interactive 等分类由 source catalog/policy 决定，文档不复制全部
widget 名单。

Style token、state evidence 和 impact 见
[`vivid_style_token_law_v0.md`](../../../docs/ui/vivid_style_token_law_v0.md)。

## Diagnostics 与验证

- machine-readable runtime event 进入 trace；文本日志走统一 output/logger；
- Scene 暴露稳定 Cmd/Exec/Tile/overflow 统计，不导出内部 payload；
- 示例用于局部行为，internal regression 可以访问内部入口，产品示例不得因此获得相同 import 权限；
- Host fixture、QEMU 和真实板属于不同 evidence domain；build success 不等于 render/input/product success。

推荐示例、evidence manifest 和专题测试从 [`docs/ui/README.md`](../../../docs/ui/README.md) 进入。
