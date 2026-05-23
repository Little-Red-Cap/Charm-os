# 入口面契约：稳定 / 兼容 / 退役

## Summary

Charm 的导入入口是架构边界的一部分，不是“哪个名字能编译就用哪个名字”。
新代码应优先使用稳定入口或明确的叶子模块；兼容入口只保留迁移语义；
退役入口只保留 tombstone，避免旧依赖静默继续工作。

本契约的目标是封住历史 facade：

- 不再让 `charm.runtime` 被误认为 Charm Runtime、RTE 或系统总入口。
- 不再让 `charm.domain` 被误认为领域层总入口。
- 不再让 `charm.foundation` 成为 first-party 新代码的默认入口。
- 不新增一个新的“大 facade” 替代 `charm.runtime`。

## 入口分类

### 稳定入口

这些是新代码优先使用的公共入口：

- `charm.core`
- `charm.system`
- `charm.io`
- `charm.net`
- `charm.media`
- `charm.ui.ink`
- `charm.ui.vivid`

如果叶子模块比聚合入口更能表达依赖意图，应直接导入叶子模块。例如：

```cpp
import fs_vfs;
import hal_uart;
import shell_cmd;
import power.core;
```

### 兼容入口

- `charm.foundation`

`charm.foundation` 当前仅作为迁移 facade 保留，并转发到 `charm.core`。
它不是新代码的默认主入口，也不是“基础层的永久总门面”。

当前仓库 first-party 源码不保留 `charm.foundation` 导入。未来如果确实需要
显式兼容样例，必须同步更新白名单契约并写明理由。

### 退役入口

- `charm.runtime`
- `charm.domain`

`charm.runtime` 已退役为 tombstone 模块，不 re-export 任何模块，并从正常
runtime source collection 中排除。它只负责让旧导入尽早、明确地失败。

`charm.domain` 是历史入口名。领域能力应使用 `charm.media`、`charm.ui.ink`、
`charm.ui.vivid` 或更窄的叶子模块表达。

## 规则

- `Modules/*`、`Examples/*`、`Draft/*` 不得新增对 `charm.foundation`、
  `charm.runtime` 或 `charm.domain` 的依赖。
- `CHARM_ENABLE_DEPENDENCY_WHITELIST=ON` 会在 CMake 配置阶段检查上述
  first-party source tree 中的历史入口导入。
- 白名单检查是 opt-in，不默认强制，避免干扰 H7-lab 与并行实验。
- `charm.runtime` 不得重新加入 `charm_collect_system_sources()` 的正常
  source collection。
- 任何入口门面都不应被当成 runtime framework、调度器、service locator
  或 dependency injection container。
- 本轮不进一步拆 `charm.system` / `charm.io` 聚合入口；只封住历史入口。
- 稳定聚合入口的宽度解释与后续治理见
  [`stable_entry_aggregate_contract.md`](stable_entry_aggregate_contract.md)。

## 迁移规则

旧导入应迁移到稳定入口或叶子模块：

```cpp
// 旧：不要继续使用
import charm.runtime;
import charm.foundation;
import charm.domain;

// 新：按真实边界选择
import charm.core;
import charm.system;
import charm.io;
import charm.net;
import charm.media;
import charm.ui.vivid;
```

如果只依赖单个能力，优先使用更窄的叶子模块，而不是制造新的聚合 facade。

## 非目标

- 不删除 `charm.foundation` 文件。
- 不新增 manifest、YAML、DSL、generator 或 graph compiler。
- 不新增 CI 强制规则。
- 不修改 H747-lab 底座。
- 不把 RTE 做成 runtime framework。

## 相关契约

- 入口白名单与检查规则：[`dependency_whitelist.md`](dependency_whitelist.md)
- 稳定聚合入口契约：[`stable_entry_aggregate_contract.md`](stable_entry_aggregate_contract.md)
- `charm.runtime` 退役细则：[`legacy_runtime_facade_retirement_contract.md`](legacy_runtime_facade_retirement_contract.md)
- Charm 主脊梁与入口语义：[`charm_spine_v0.md`](charm_spine_v0.md)
- RTE 能力装配边界：[`rte_capability_composition_contract_v0.md`](rte_capability_composition_contract_v0.md)
