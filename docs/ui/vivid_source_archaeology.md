# Vivid 源码考古记录

## 文档状态

- `status`: `supporting-exploration`
- `scope`: 固定 Vivid 源码事实、审计工具、archive WIP 与消费者证据
- `authority`: 源码与 Profile Compiler；不修改 Charm Core canonical 语义

本记录不是 `OnlyVivid` 或 Vivid 产品基线。机器可读明细见
[`vivid_source_archaeology_manifest.json`](vivid_source_archaeology_manifest.json)，可重复审计脚本见
[`scripts/vivid_source_archaeology.ps1`](../../scripts/vivid_source_archaeology.ps1)。

## 固定对象

| 角色 | 精确对象 |
|---|---|
| Vivid 源码事实底 | `dev@95759c7647606afb0a740f76f581bf2a198bde2c` |
| Core 语义裁决底 | `OnlyCore@9a7ef5db185564e4f1cff853916cd05856597f02` |
| WIP 原料 | `archive@40f610cbd4fea55e9e1ca37bcbf4bd616d81a30d` |
| 差异共同祖先 | `0cdfbbdc29b3ef583c4f774928a18bda408b6068` |

archive 已推送为 `origin/archive/pre-onlycore-noncore-wip`，并由 annotated tag
`archive-pre-onlycore-noncore-wip` 固化。旧独立 `Charm-vivid` 仅作为历史参照。

## 双快照结果

两份快照均为 177 个 Vivid 文件，其中 163 个 `.cppm`、163 个 module。

| 指标 | dev | archive |
|---|---:|---:|
| import edges | 904 | 904 |
| unique imports | 100 | 100 |
| 内部 import edges | 755 | 755 |
| 目录外 import edges | 149 | 149 |
| lexical frontier | 25 | 25 |
| SCC | 163 个单点 | 163 个单点 |
| cycle | 0 | 0 |

Policy 分类必须区分显式和默认归属：30 个 `PRODUCT_ROOT`、2 个 `HOST_ONLY`、11 个
`INTERNAL_EXPLICIT`，以及 120 个 `INTERNAL_BY_DEFAULT`。

Profile Compiler 观察到的 `player_md3` 闭包为 69 modules / 68 sources，debug 闭包为
73 modules / 72 sources。profile fingerprint 为
`180caf1e6f46b2d82fc54761a25e81317e00283b7d71fc3499f60000a9876720`；Host 与 H747 target
fingerprint 分别为 `222d40c405fef09c572672d9015718164b071cac6edcc912114fefc6d0f41c23` 和
`2baf4844112aeb8ae0588fb47c7eb942d86e1025f24443ea16a5157b5c1561be`。

Profile-aware external requirements 与全量 lexical frontier 分开记录。全量 Vivid frontier
为 25 个，`player_md3` 为 16 个，debug 为 17 个；每个名称的源码 owner、generated 或
unresolved 状态写入 manifest。生成 closure 后的 normalized requirements 当前为 4 个，不能
替代全量拆仓边界。

## Archive patch queue

archive 相对共同祖先的 Vivid 语义差异来自 `0cdfbbd..40f610c`，共 10 个文件：

- `Examples/ui/vivid/page_transition_demo/main.cpp`
- `Modules/ui/vivid/ARCHITECTURE.md`
- `Modules/ui/vivid/core/layer_runtime.cppm`
- `Modules/ui/vivid/core/motion_compose.cppm`
- `Modules/ui/vivid/core/motion_execute.cppm`
- `Modules/ui/vivid/core/scene.cppm`
- `Modules/ui/vivid/core/scene_snapshot_store.cppm`
- `docs/ui/ui_kernel_contract.md`
- `docs/ui/vivid_import_boundary_contract.md`
- `docs/ui/vivid_layer_runtime_v0.md`

这些文件目前均为 `Quarantined`，不是自动接受的补丁。必须继续按 semantic hunk 拆分，记录
意图、依赖、验证和 `Accept / Reject / Deferred / Quarantined` 裁决。archive page-transition
demo 段错误，因此 `LayerComposePlan` 改动暂定 `Quarantined / causality pending`；段错误本身
不能证明该改动就是根因。

## 证据矩阵

| 切片 | 当前状态 | 说明 |
|---|---|---|
| `gfx-min` | `pending` | DrawCmd、Canvas、Framebuffer、FullFrame/Tile 的窄闭包尚未单独建立 |
| `scene-min` | `pending` | Scene、最小 state/invalidation 到 DrawCmd/artifact 尚未单独建立 |
| `player-pressure` | `dev snapshot passed`, `clean clone hung`, `archive failed` | 导出的精确 dev 快照 page-transition 通过；干净 clone 重跑在构建后无输出并终止；archive 段错误；两份 static-memory 通过 |

当前 full-runtime demo 只能证明 Charm 全仓消费者行为，不能证明 Vivid 可独立拆仓。干净 clone 的重跑结果
单独记录为挂起，不覆盖此前精确 dev 快照的通过证据。

## 审计工具固定

以下工具均固定为 dev 快照中的 blob SHA，变更工具必须更新本记录和 manifest：

| 文件 | blob SHA |
|---|---|
| `Modules/ui/vivid/cmake/product_profile_compiler.cmake` | `b26247503f68a7e7db9cfab8e111c66575419b25` |
| `Modules/ui/vivid/cmake/vivid_module_policy.cmake` | `a9e5e2ccba59af795d11b42a607e13f9b9eae1ee` |
| `Modules/ui/vivid/cmake/widget_catalog_compiler.cmake` | `109726abbdec60e295474f6e2a1fc9b8d2b83056` |
| `scripts/vivid_product_profile_compiler_smoke.ps1` | `d30aa5e1419d31c1b677853a7f713163d6820092` |
| `scripts/vivid_stack_usage_gate_smoke.ps1` | `652363bba94879c10be797918daf40030720cc5f` |
| `scripts/vivid_static_memory_admission_smoke.ps1` | `32901286214686535fb13cdcb0647c08a1822937` |

## 当前裁决

- `dev` 是 Vivid 源码候选底，不等于 Core 语义裁决底。
- archive 是可追溯的 WIP 原料，不是 dev 的干净后继。
- 不创建 `OnlyVivid`，不建立独立 Vivid 仓库，不恢复旧仓库，不把 `relations.hpp` 接入 Vivid。
- 在 `gfx-min`、`scene-min`、`player-pressure` 和 OnlyCore 兼容审查完成前，不创建
  `vivid-distillation-source` 或 `vivid-distillation-baseline` 标签。
