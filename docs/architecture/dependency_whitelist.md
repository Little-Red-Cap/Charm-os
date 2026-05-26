# Dependency Whitelist（入口面构建检查契约）

## Summary

`DependencyWhitelist.cmake` 是一个 opt-in 的配置期检查，用来把历史入口面封住。
它不是完整的依赖图编译器，也不是默认 CI 强制规则。

当前启用方式：

```powershell
cmake -S . -B cmake-build-whitelist-check -G Ninja -DCHARM_ENABLE_DEPENDENCY_WHITELIST=ON
```

默认构建不启用该检查，避免干扰 H7-lab、实验分支和局部样例的快速推进。

## Blocked Entry Imports

检查会拒绝 first-party checked source tree 中的历史 / 兼容入口导入：

- `charm.foundation`：兼容入口，仅保留迁移语义；first-party 新代码不得依赖。
- `charm.runtime`：退役入口，仅保留 tombstone；不得作为可用入口。
- `charm.domain`：历史入口，已由 `charm.media` 与 `charm.ui.*` 替代。

被检查的目录范围：

- `Modules/*`
- `Examples/*`
- `Draft/*`

检查覆盖常见 C++ 源码与模块后缀：`.cpp`、`.cxx`、`.cc`、`.hpp`、
`.hxx`、`.hh`、`.h`、`.cppm`、`.ixx`、`.mpp`、`.mxx`。
构建产物目录 `cmake-build-*` 与 `Modules/thirdparty/*` 会被排除，路径排除兼容
Windows 与 Unix 风格分隔符。
检查按行执行，并要求模块名边界匹配：`charm.runtime_extra` 不会被当作
`charm.runtime` 违规；真正违规时会报告 `file:line` 与对应 import 行。

推荐使用的稳定入口见 [`entry_surface_contract.md`](entry_surface_contract.md)：

- `charm.core`
- `charm.system`
- `charm.io`
- `charm.net`
- `charm.media`
- `charm.ui.ink`
- `charm.ui.vivid`

如果叶子模块能更准确表达依赖意图，应直接导入叶子模块。

## Kernel Presentation Boundary

同一个 opt-in 检查还保留 kernel core 的 presentation 边界规则：

- `Modules/system/kernel/*.cppm` 不得导入 `out.*`。
- `*_export.cppm` 例外，用于承接 JSON、CSV、text 等 presentation/export 逻辑。
- 如果 port adapter 明确承担 sink-oriented I/O 桥接职责，可在对应 adapter 层依赖 `out.*`，但不要把它拉回 kernel core。

这条规则服务于 scheduler 职责收口：调度器不是报表中心，也不是导出器。

## Stable Entry Hygiene

同一个 opt-in 检查会先维护一份 `charm.*.cppm` 入口台账。新增
`Modules/**/charm.*.cppm` 时，必须在台账中明确分类；否则 CMake 配置阶段失败。
这条规则用于防止新的半公开 facade 悄悄长出来。

当前公共稳定聚合入口：

- `Modules/core/charm.core.cppm`
- `Modules/system/charm.system.cppm`
- `Modules/io/charm.io.cppm`
- `Modules/io/charm.net.cppm`
- `Modules/media/charm.media.cppm`
- `Modules/media/charm.media.audio.cppm`
- `Modules/ui/ink/charm.ui.ink.cppm`
- `Modules/ui/vivid/charm.ui.vivid.cppm`

当前已分类的非稳定入口：

- `Modules/core/charm.foundation.cppm`：兼容迁移 facade。
- `Modules/system/charm.runtime.cppm`：退役 tombstone。
- `Modules/ui/common/charm.core.event.cppm`：公共叶子入口，不是稳定聚合入口。
- `Modules/ui/vivid/charm.ui.vivid_internal.cppm`：内部入口，不对外推荐。

检查规则：

- 台账中的公共稳定聚合入口文件必须存在；缺失说明契约或源码已漂移。
- 新增 `Modules/**/charm.*.cppm` 必须同步更新台账分类。
- 稳定入口不得 re-export `charm.foundation`、`charm.runtime` 或 `charm.domain`。
- 稳定入口不得 re-export 名称包含 `internal`、`bridge`、`compat` 或 `alias` 的私有/过渡表面。

这里刻意区分“入口台账检查”和“稳定入口卫生检查”。`Modules/**/charm.*.cppm`
会被 inventory guard 分类，但不会全部套用稳定入口 re-export 规则；`charm.*`
文件名只能说明它是某种入口或命名空间门面，不能自动证明它是公共稳定聚合入口。

这不是限制稳定入口必须很小，而是避免它们重新长成新的 zombie facade。
稳定聚合入口的宽度解释见 [`stable_entry_aggregate_contract.md`](stable_entry_aggregate_contract.md)。

## Execution Rules

1. 新代码不得导入 blocked entry imports。
2. 当前没有 first-party 例外；如果未来确需兼容样例，必须更新本契约并写明理由。
3. 违规会在 CMake 配置阶段失败。
4. 不通过新增聚合 facade 替代 `charm.runtime`。

## Examples

禁止：

```cpp
import charm.foundation;
import charm.runtime;
import charm.domain;
```

推荐：

```cpp
import charm.core;
import charm.system;
import charm.io;
import charm.net;
import charm.media;
import charm.ui.vivid;
```

或更窄的叶子模块：

```cpp
import fs_vfs;
import hal_uart;
import shell_cmd;
```

## References

- 入口面契约：[`entry_surface_contract.md`](entry_surface_contract.md)
- Runtime facade 退役契约：[`legacy_runtime_facade_retirement_contract.md`](legacy_runtime_facade_retirement_contract.md)
- 总架构说明：[`../architecture_overview.md`](../architecture_overview.md)
- 检查实现：[`../../cmake/DependencyWhitelist.cmake`](../../cmake/DependencyWhitelist.cmake)
