# 稳定聚合入口契约

## Summary

稳定聚合入口可以宽，但必须可解释。它们不是兼容门面、不是产品 preset、
也不是把旧 `charm.runtime` 换个名字继续存在。

本契约约束 `charm.core`、`charm.system`、`charm.io`、`charm.net`、
`charm.media`、`charm.ui.ink`、`charm.ui.vivid` 这类长期入口。
`charm.media.audio` 当前作为 `charm.media` 公开 re-export 的子聚合入口，
同样纳入稳定入口卫生检查，但不升级为新的顶层推荐入口。

## 入口类型

### 基座入口

- `charm.core`

允许聚合 util、init、trace、service、algorithm 等跨层基础能力。
它不应导出 Runtime、IO、Domain 或 board/backend 能力。

### 子系统入口

- `charm.system`
- `charm.io`
- `charm.net`
- `charm.media`
- `charm.media.audio`
- `charm.ui.ink`
- `charm.ui.vivid`

允许聚合同一子系统内的公共能力。它们可以宽，但宽度必须服务于明确的
开发入口，而不是隐藏真实依赖。`charm.media.audio` 属于 media 子系统的
公开子聚合，用于表达音频能力边界；默认推荐仍优先从 `charm.media` 或更窄叶子模块进入。

### 非稳定入口

- `charm.foundation`：迁移 facade，不作为 first-party 默认入口。
- `charm.runtime`：退役 tombstone，不得重新导出模块。
- `charm.domain`：历史入口名，不再存在为领域层总入口。
- `*_internal`、`*bridge*`、`*compat*`、`*alias*`：不得作为稳定聚合入口对外推荐。

## 规则

- 稳定聚合入口不得 re-export 历史入口：`charm.foundation`、`charm.runtime`、`charm.domain`。
- 稳定聚合入口不得通过 internal / bridge / compat / alias 模块扩大公共表面。
- 如果一个能力只服务于单一示例、单一 board 或单一产品 profile，优先使用叶子模块或产品入口，不要塞进稳定聚合入口。
- 如果聚合入口继续变宽，必须能回答它属于基座能力、子系统公共能力、还是产品/场景入口。
- 叶子模块能更清楚表达依赖时，优先使用叶子模块。

## 当前观察

- `charm.core` 当前偏向基础能力包，宽度主要来自 service/container/algorithm。
- `charm.system` 当前偏向系统底座包，包含 clock/init/bringup/boot/device。
- `charm.ui.vivid` 当前偏向产品级 UI 入口，包含 scene/gfx/font 公开面。

这些入口可以先保留，但后续治理应优先解释和压缩仍然偏宽的稳定聚合入口，
而不是让历史入口回流。

## 非目标

- 本轮不拆分 `charm.core`、`charm.system` 或 `charm.ui.vivid`。
- 本轮不删除 `charm.foundation`。
- 本轮不新增 manifest、DSL、generator 或 preset compiler。
- 本轮不修改 H747-lab、UI/Vivid 编译阻塞或 RTE smoke。
