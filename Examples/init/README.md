# Init / Observe 示例入口

本目录收纳 bring-up、materialize、connection observe 等装配观察链示例。

它们更适合回答：

- 某个 `Plan` 或 bring-up helper 能不能 materialize 成稳定结果
- `materialized_graph` 能不能被导出、观察和批量验证
- 当前观察链路对应的最小示例从哪里进入

如果你想先看现行设计，再看示例，建议先读：

- [`../../docs/system/init_graph_contract.md`](../../docs/system/init_graph_contract.md)
- [`../../docs/system/init_materialized_graph_observe.md`](../../docs/system/init_materialized_graph_observe.md)
- [`../../schemas/README.md`](../../schemas/README.md)

## 当前示例

### `materialize_observe_demo`

入口：

- [`materialize_observe_demo/README.md`](materialize_observe_demo/README.md)

适合先看最小 `Plan -> materialized_graph -> DOT / JSON` 导出链。

### `bringup_minimal_observe_demo`

入口：

- [`bringup_minimal_observe_demo/README.md`](bringup_minimal_observe_demo/README.md)

适合看更接近系统入口 helper 的观察导出路径。

### `bringup_block_observe_demo`

目录：

- `bringup_block_observe_demo/`

更适合在你已经在看 block / bring-up 组合时直接进入。

### `connection_observe_demo`

入口：

- [`connection_observe_demo/README.md`](connection_observe_demo/README.md)

适合看 connection / observe 视角的最小示例。

## 建议阅读顺序

1. `materialize_observe_demo`
2. `bringup_minimal_observe_demo`
3. `connection_observe_demo`
4. `bringup_block_observe_demo`

## 使用提醒

- 这些示例主要服务观察链、导出链和 bring-up 证据链，不是通用应用示例。
- 如果你需要先判断“当前系统装配规则是什么”，先回到 `docs/system/*` 再进本目录。
