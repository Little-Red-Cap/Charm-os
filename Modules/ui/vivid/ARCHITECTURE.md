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
- `charm.gfx.host_tools`：host snapshot/DrawCmd 证据工具，不进入默认 PRODUCT profile；
- SoA kernel、DrawCmd partitions 与 `charm.ui.vivid_internal`：实现入口，产品不得直接 import。

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
- command/text/blob/workspace 容量固定，耗尽必须产生显式 sticky evidence，不静默截断；
- FullFrame 与 Tile/PFB 消费同一 record 语义；不同 backend 结果需要一致性证据；
- command snapshot 复制 buffer/payload，不复制临时 workspace；
- compaction 在 record 完成后执行，不能改变命令可观察顺序或 artifact 语义；
- tile 命中索引容量不足时必须回退到正确但更慢的扫描路径；
- 产品 evidence 只消费 Scene stats 和 artifact 摘要，不依赖 CmdHeader/payload 私有布局。

`Scene` 的 builder/layer support 是附属实现层，render detail 是 private partition；它们不建立新的产品或
evidence surface。DrawCmd 与 state-to-artifact evidence 分别见：

- [`vivid_draw_cmd_evidence_boundary_v0.md`](../../../docs/ui/vivid_draw_cmd_evidence_boundary_v0.md)；
- [`vivid_render_evidence_chain_v0.md`](../../../docs/ui/vivid_render_evidence_chain_v0.md)。

## Workspace 与静态内存

SoA traversal、layout、render、input、semantic、compaction 和 raster scratch 使用 Scene/调用方拥有的固定
workspace。共享 workspace 只允许单 UI execution domain 内串行使用；同一对象上的并发或重入必须被
拒绝或由调用方隔离。

PRODUCT/MCU profile 必须声明：

- SoA node/payload、TextArena、Style、DrawCmd 和 layer cache 容量；
- Scene 数量、常驻 RAM 上限、最小 headroom 与 stack frame 上限；
- screen/pixel format 和 backend envelope；
- overflow、workspace exhaustion 和 payload generation 的失败行为。

`-fstack-usage` 只能约束单函数 frame，不能证明调用链峰值。产品任务栈仍需入口分析或运行时
high-water evidence。静态内存准入见
[`vivid_static_memory_admission.md`](../../../docs/ui/vivid_static_memory_admission.md)。

## Payload、Catalog 与 PRODUCT Profile

Widget node 只保存 kind 与 generation-checked payload handle。每类 payload 使用固定容量 pool；释放后
generation 变化，旧 handle 不得重新命中新 owner。

`cmake/widget_catalog.cmake` 是 WidgetKind、module、factory、payload、style/default/input behavior 的
单一构建入口。稳定 kind ID 由 ABI fixture 固定；PRODUCT profile 只裁剪能力和容量，不生成另一套 enum。

Product Profile Compiler 是 Vivid 工具，不是 Charm Core 或产品 C++ API：

- C++ `module/import/export import` 是依赖边唯一来源；
- CMake policy 只标记 product root、internal、host-only 和硬件 envelope；
- profile 固定产品 root、active WidgetKind 和工作集；target envelope 固定设备资源；
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
`Style`、nine-slice skin 或调试型拖动文本。逐实例差异使用既有 style variant/rule；图片化外观应由显式
装饰控件组合，而不是让每个基础滚动容器常驻未使用的 image/font 状态。

高频 `Button` 同样不保存 instance-local `Style` 或 nine-slice skin；背景、边框、字体和 focus ring 统一从
Theme/StyleSheet 解析。icon 是按钮的显式内容能力，仍由选择该能力的实例持有；逐实例视觉差异使用既有
style variant/rule，图片化背景通过显式装饰组合实现。

`ObjectBase` 同样不常驻子节点数组。`WidgetBase<Derived, ChildCapacity>` 的默认容量为零；固定 inline 模式
只供明确选择编译期容量的自定义控件。内置 List、FoldablePanel、ScrollContainer 使用
`std::dynamic_extent` 外部模式，由调用方通过 `attach_child_storage(std::span<WidgetHandle>)` 提供实际容量；
未 attach 时容量为零且 add/insert 返回 `false`。storage 必须独占并覆盖容器使用期，detach 会清空 active
handle，容器析构也会清空仍 active 的 handle。容量耗尽显式返回 `false`，不分配动态内存；ScrollContainer
的 resolver 在一次调用内必须稳定。

`FoldablePanel` 的标题和正文同样按需借用调用方提供的 NUL 结尾文本，不在每个实例中预留固定文本缓冲。
文本 owner 必须覆盖控件的完整绘制周期；传入 `nullptr` 等价于空字符串。需要独立文本所有权时，由上层状态或
专用文本存储承担，不能把最大内容容量重新乘到每个折叠面板实例上。

`Label` 及组合它的 object widget 借用调用方文本，并在 setter 时记录最多 64 字节的 admitted range；测量与
绘制都使用该显式长度，不重新做无界 NUL 扫描。owner 必须覆盖控件使用期；原地修改文本后必须再次调用 setter
刷新长度和几何。需要更长或自有文本时，应使用 SoA TextArena 或显式的上层文本存储，而不是恢复逐实例缓冲。

跨帧保存的 UI callback 使用 `util::delegate` 并由 owner 保证 target 生命周期。历史名称 `Callback` 只是
`util::delegate<>` 的单轨别名，不得重新增加独立的 `fn + void*` fallback 或第二份 callback 存储。
Button、ListItem、MenuItem 这类单一 command edge 每个实例只保存一个 delegate；需要同步多播时，由调用方
显式拥有 `service::signal` 并从该 delegate 转发，不能让所有控件预付固定槽表。truth/value widget 是否保留
`service::state` 由其可读状态与真实 observer 消费证据单独裁决，不能与 command edge 混为一谈。
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
