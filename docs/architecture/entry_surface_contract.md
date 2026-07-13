# 入口面契约

## 文档状态

- `status`: `supporting`
- `scope`: `charm.*` module 入口分类与聚合卫生
- `source`: [`DependencyWhitelist.cmake`](../../cmake/DependencyWhitelist.cmake)

导入入口是依赖边界，不因名称可用就成为稳定 API。新代码应优先导入能准确表达意图的
叶子 module；确实需要一组子系统公共能力时，才使用稳定聚合入口。

## 入口分类

### 稳定入口

当前由 CMake 台账检查的稳定入口是：

- 基座入口：`charm.core`
- 子系统入口：`charm.system`、`charm.io`、`charm.net`、`charm.media`
- media 子聚合：`charm.media.audio`
- UI 子系统入口：`charm.ui.ink`、`charm.ui.vivid`

稳定只表示入口受台账和导出卫生约束，不表示其内容自动获得 Charm Core 身份。
`charm.media.audio` 是公开子聚合，也受相同检查，但不是新的顶层总入口。

### 兼容入口

- `charm.foundation`：迁移 facade，当前转发到 `charm.core`。

它不供 first-party 新代码导入，也不是永久基础层总门面。保留文件只维持明确的兼容边界。

### 退役入口

- `charm.runtime`：tombstone module，不 re-export module，也不进入正常 runtime source collection。
- `charm.domain`：历史入口名，当前没有对应 module 文件。

领域能力应通过 `charm.media`、`charm.ui.ink`、`charm.ui.vivid` 或更窄的叶子 module 表达；
不得创建新的大 facade 来替代这些退役入口。

### 内部入口

- `charm.core.event`
- `charm.ui.vivid_internal`

它们位于 CMake 非稳定台账中，不得因 `charm.*` 命名而被推荐为公共聚合入口。

## 聚合卫生

- 稳定入口不得 re-export `charm.foundation`、`charm.runtime` 或 `charm.domain`。
- 稳定入口不得 re-export 名称包含 `internal`、`bridge`、`compat` 或 `alias` 的过渡表面。
- 单一示例、board、backend 或产品 profile 的能力不得进入稳定聚合入口。
- 叶子 module 能更清楚表达依赖时，优先导入叶子 module。
- 聚合入口扩宽时，必须说明新增内容为何属于基座或该子系统的公共能力。

这些规则约束导出表面，不证明完整 module DAG，也不把聚合入口解释为 runtime framework、
service locator 或 dependency injection container。

## 自动检查边界

启用 `CHARM_ENABLE_DEPENDENCY_WHITELIST=ON` 后，CMake 会：

- 检查 `Modules`、`Examples`、`Draft` 中的 first-party source 不导入兼容或退役入口；
- 要求 `Modules/**/charm.*.cppm` 进入稳定或非稳定台账；
- 检查稳定入口文件存在，并执行上述 re-export 卫生规则。

检查默认关闭，且不验证完整依赖图、runtime 行为或某个概念的 Core 准入资格。具体实现与启用方式见
[`dependency_whitelist.md`](dependency_whitelist.md)。

## 相关契约

- 依赖边界：[`dependency_contract.md`](dependency_contract.md)
- Charm Core 准入：[`../../CONSTITUTION.md`](../../CONSTITUTION.md)
