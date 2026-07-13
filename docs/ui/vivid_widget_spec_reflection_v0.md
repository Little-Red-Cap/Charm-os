# Vivid WidgetSpec Reflection v0

本文定义 Vivid UI 在 C++26 static reflection 真正进入主线前的前置契约。目标不是现在就依赖 GCC16 `<meta>` 语法，而是先把控件 API 森林收束成一个反射无关的 `WidgetSpec` 元模型，再让手写 `constexpr` 与未来反射生成共用同一个形状。

## 目标

- 先固定控件元信息的结构，而不是继续围绕 `SoaKernel` / `SceneBuilder` / `SoaGui` 手写更多平行 API。
- 当前工具链使用手写 `constexpr WidgetSpec` 作为后端。
- GCC16 反射可用后，在实验 feature flag 下生成同形 `WidgetSpec`，再比较两种后端的输出。
- `charm.ui.vivid` 不直接依赖实验性 `<meta>`，反射适配只允许存在于实验模块或显式开关下。

## 非目标

- 本阶段不改运行时行为。
- 本阶段不改现有 builder、payload mutator、render recorder、input dispatch 的签名。
- 本阶段不把 `WidgetSpec` re-export 到 `charm.ui.vivid` façade。
- 本阶段不先处理 `ListView` / `TableView` / `TreeView`，避免结构化视图把第一刀拖入大矩阵。

## 元模型

`WidgetSpec` 至少描述这些信息：

| 字段 | 含义 |
| --- | --- |
| `kind` | 对应 `WidgetKind` |
| `payload` / `payload_type` | 对应 SoA payload kind 与 C++ payload 类型名 |
| `builder_name` / `factory_name` | builder DSL 暴露名与当前 factory 入口 |
| `source` | 当前 `ManualConstexpr`，未来可为 `StaticReflection` |
| `properties` | 属性列表，包括名称、值类型、setter/getter、默认值与说明 |
| `dirty` | 属性变更触发 `None` / `Paint` / `Layout` / `LayoutAndPaint` |
| `state_influence` / `layout_state_mask` | 交互状态是否可能影响 layout；当前试点均为 paint-only |
| `semantic_role` / `semantic_actions` | 默认语义角色与 action mask |
| `default_focusable` / `default_hit_test` / `default_clip_children` | 创建默认行为 |

当前落点是 `Modules/ui/vivid/core/widget_spec.cppm`，模块名为 `charm.core.widget_spec`。它是实验/契约模块，不是 public façade。

## 试点族

第一批试点选择中等复杂度控件：`SegmentedControl`、`Stepper`、`Switch`、`Slider`。

| Widget | Payload | Builder/Factory | 默认 | 属性与 dirty |
| --- | --- | --- | --- | --- |
| `SegmentedControl` | `SegmentedControlPayload` | `segmented_control` / `create_segmented_control` | focusable=true | `count`: layout+paint；`label[index]`: layout+paint；`selected`: paint |
| `Stepper` | `StepperPayload` | `stepper` / `create_stepper` | count=3,current=0 | `count`: paint；`label[index]`: paint；`current`: paint |
| `Switch` | `SwitchPayload` | `switch` / `create_switch` | semantic role Button | `checked`: paint |
| `Slider` | `SliderPayload` | `slider` / `create_slider` | value=0,min=0,max=100 | `value`: paint；`min_value/max_value`: layout via `set_range` |

这组能覆盖：

- index 属性：`label[index]`
- clamp 语义：`selected/current/value`
- shared mutator：`set_checked` / `set_value` / `set_range`
- input 行为：toggle、segmented index、stepper index、slider drag
- render 行为：状态变更只重绘，不触发布局状态影响位

## 双后端策略

### 当前后端：ManualConstexpr

手写 `constexpr WidgetSpec` 用来验证元模型是否足以表达现有 API。验收标准是：一个试点控件的 payload、默认值、属性、dirty 策略、语义默认与 builder 暴露名都能被 spec 完整描述。

### 未来后端：StaticReflection

GCC16 可用后新增实验模块，例如 `charm.core.widget_spec.reflection_experiment`，只在显式 feature flag 下编译。该模块负责：

1. 从 payload 类型、属性标注或轻量 trait 生成 `WidgetSpec`。
2. 输出与 `ManualConstexpr` 后端同形的数据。
3. 对试点族做 static compare，先证明反射后端没有改变契约。

反射后端不得直接被 `charm.ui.vivid` import；只有当 spec 已稳定且 host/MCU 工具链策略明确后，才讨论是否晋升为生成链路。

## 迁移边界

后续主线 API 重构应从 spec 派生或约束这些面：

- `SoaFactory::create_*`
- `SoaKernel::set_*` / getter mutator 面
- `SceneAccess` / `SceneBuilder` 的 DSL 暴露面
- input action 到 payload mutator 的路由
- render recorder 对 payload 字段的读取形状

但迁移顺序必须保持“先证明、再替换”：

1. 手写 spec 完整覆盖试点族。
2. 增加静态契约检查或最小 demo 证明 spec 与现有行为一致。
3. 让一个试点控件的 builder/mutator 受 spec 约束。
4. GCC16 后接入反射生成同形 spec。
5. 再扩大到更多控件族。

## 风险与护栏

- `cmake/widget_catalog.cmake` 已是 WidgetKind ABI、module/factory、payload、style/defaults 和 input behavior 的构建期单一源；`WidgetSpec` 不得重新手写这些字段形成第二套 catalog。
- `WidgetSpec` 当前只做运行时/API 契约试点。它要进入主线，必须从 catalog 派生，或用静态比较证明与 catalog 一致；如果长期不能驱动生成或约束代码，就应回收。
- 不允许用大量宏模拟反射；手写后端必须保持 constexpr 数据结构，反射后端另行隔离。
- 结构化视图族必须等试点闭环后再进入，因为其虚拟化、回调、滚动和列/树模型会显著扩大模型复杂度。

## 验证

当前阶段使用：

```powershell
cmake --preset host-debug
cmake --build --preset host-debug -j 22
```

通过标准：

- `charm.core.widget_spec` 能随 `Charm-ui` 编译。
- `charm.ui.vivid` public surface 不变。
- 现有 Vivid 主链无需新增 import。
- `SegmentedControl`、`Stepper`、`Switch`、`Slider` 的现有 API 能由 spec 表达。
