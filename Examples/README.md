# Core 示例入口

OnlyCore 示例只用于验证已准入的 Core 关系语义和消费边界。单个示例通过不代表全部出炉条件成立，
也不代表外围运行时或板级能力可用。

## 当前证据

| 示例 | 用途 |
|---|---|
| [`system/charm_capability_relations`](system/charm_capability_relations/README.md) | `Requirement`、`Provision`、`Binding` 和解析失败关系的 Host 验证 |
| [`system/charm_capability_mvp`](system/charm_capability_mvp/README.md) | Core 能力模型的最小组合证据 |
| [`system/charm_core_external_consumer`](system/charm_core_external_consumer/README.md) | 安装后通过 `find_package` 消费 `Charm::core` |
| [`system/charm_core_arm_build_only`](system/charm_core_arm_build_only/README.md) | ARM freestanding 编译期消费边界 |

本地构建目录使用仓库约定的 `cmake-build-*` 命名。仓库总入口见
[根 README](../README.md)，OnlyCore 提纯流程见
[`only_core_distillation_sop.md`](../docs/project/only_core_distillation_sop.md)。
