# Legacy Runtime Facade 退役契约

## Summary

`charm.runtime` 已退役为可用入口模块。它不是 Charm runtime，不是 RTE，
也不是子系统边界；它只作为 tombstone 模块保留，让陈旧导入尽早、明确失败，
而不是继续静默 re-export 半个仓库。

替代规则刻意保持简单：

```cpp
import charm.system; // system 子系统聚合入口
import charm.io;     // IO 子系统聚合入口
import charm.net;    // net 子系统聚合入口
```

或直接导入更窄的叶子模块：

```cpp
import fs_vfs;
import hal_uart;
import shell_cmd;
import power.core;
```

## Rules

- 新代码不得导入 `charm.runtime`。
- 现有示例应迁移到明确的子系统入口或叶子模块。
- `Modules/system/charm.runtime.cppm` 不得重新 re-export 模块。
- 正常 runtime source collection 必须排除 `charm.runtime.cppm`。
- `CHARM_ENABLE_DEPENDENCY_WHITELIST=ON` 会拒绝 checked source tree 中的
  `import charm.runtime;`。

## Rationale

旧 facade 虽然被标注为 legacy compatibility，但它曾导出 System、IO、Net、
FS、HAL、Shell、Out、block device、driver 与 protocol 模块。这个形状会让
临时迁移层变成事实上的永久产品入口。

Charm 的入口面应该保留清晰语义：

- `charm.core` 是核心能力基座。
- `charm.system` 是系统能力聚合入口。
- `charm.io` 与 `charm.net` 是 IO-facing 聚合入口。
- 叶子模块表达精确依赖意图。

`charm.runtime` 不能清晰表达这些边界，因此不应再成为可工作的导入路径。

## Migration Examples

文件系统示例：

```cpp
import charm.core;
import fs_core;
import fs_ramfs;
import fs_vfs;
```

HAL 示例：

```cpp
import charm.core;
import hal_core;
import hal_gpio;
import hal_timer;
import hal_uart;
```

Boot / system 示例：

```cpp
import charm.core;
import charm.system;
```

## Non-Goals

- 本契约不删除 `charm.foundation`；它是独立的兼容入口面。
- 本契约不要求一步缩小所有聚合入口。
- 本契约不引入 manifest、DSL、generator 或 RTE runtime。
- 本契约不修改 H747-lab 底座。
