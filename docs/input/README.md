# Input 文档入口

输入平台边界见 [`input_layering_decision.md`](input_layering_decision.md)。IO primitive
规则见 [`../io/README.md`](../io/README.md)。

当前实现入口：

- [`../../Modules/io/hal/hal_input.cppm`](../../Modules/io/hal/hal_input.cppm)：raw driver；
- [`../../Modules/io/input/input.raw_event.cppm`](../../Modules/io/input/input.raw_event.cppm)：
  跨 backend 的 raw event；
- [`../../Modules/io/input/input.service.cppm`](../../Modules/io/input/input.service.cppm)：
  clock + sampler；
- [`../../Modules/io/input/input.pump.cppm`](../../Modules/io/input/input.pump.cppm)：
  有预算的 EDA pump；
- [`../../Modules/io/input/input.router.cppm`](../../Modules/io/input/input.router.cppm)：
  raw event 分发。

旧 VSF 位段映射未被当前 `RawInputEvent` 采用，已从默认文档移除；需要追溯时使用
Git 历史或 [`../reference/vsf/README.md`](../reference/vsf/README.md)。
