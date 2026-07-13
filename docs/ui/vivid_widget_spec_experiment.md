# Vivid WidgetSpec Experiment

## 文档状态

- `status`: `exploration`（deferred）
- `scope`: 手写 `WidgetSpec` 元数据试验及继续推进条件
- `authority`: [`widget_spec.cppm`](../../Modules/ui/vivid/core/widget_spec.cppm)、
  [`widget_catalog.cmake`](../../Modules/ui/vivid/cmake/widget_catalog.cmake)

## 当前事实

`charm.core.widget_spec` 定义反射无关的 `WidgetSpec`/`PropertySpec`，并手写描述
SegmentedControl、Stepper、Switch、Slider。字段覆盖 kind、payload、builder/factory 名、property dirty
policy、semantic default 和基础 interaction flags。

当前状态仅为自校验 metadata fixture：

- spec source 是 `ManualConstexpr`；
- module 内 static_assert 只验证四个条目和关键 property 存在；
- 当前没有其它源码 import/consumer；
- `charm.ui.vivid` 不导出该模块；
- spec 不驱动 builder、mutator、input、render、catalog 或 code generation。

因此它不能证明现有 widget API 已由统一元模型约束，也不是 public contract。

## 与 Catalog 的边界

`widget_catalog.cmake` 是 WidgetKind ABI、module/factory、payload、style/default 和 input behavior 的当前构建
真源。`WidgetSpec` 不得平行手写同一信息后宣称成为第二真源。

继续推进必须选择并证明一种关系：

1. spec 从 catalog/source 派生；
2. spec 驱动至少一个真实 consumer；
3. static comparison 能拒绝 catalog/spec 漂移。

如果长期只有自描述数组和 static_assert，应删除 experiment，而不是扩充到更多 widget。

## Reflection 边界

C++ static reflection 只是可能的 metadata producer，不改变上述 consumer/ownership 要求。引入 reflection
前必须满足：

- 工具链通过显式实验 target 隔离，不进入默认 MCU/product façade；
- reflection 与 manual/catalog projection 输出同形数据并可比较；
- 有真实 consumer 证明生成价值，不为使用 `<meta>` 而新增 schema；
- failure、toolchain fallback 和 generated artifact ownership 明确。

当前没有 reflection backend、feature flag 或迁移承诺。

## 非目标

- 不修改当前 runtime behavior、builder/mutator 签名或 WidgetKind ABI；
- 不把四个试点推广成所有 widget 已覆盖；
- 不建立结构化视图、layout、render 或 semantic 的第二套 schema；
- 不维护试点扩展顺序、GCC 版本排期或 build 命令。
