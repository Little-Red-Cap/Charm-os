# Vivid 静态内存准入

## 文档状态

- `status`: `supporting implementation contract`
- `scope`: Vivid resident RAM、profile/envelope 与 hot stack admission
- `authority`: [`vivid.cmake`](../../Modules/ui/vivid/vivid.cmake)、
  [`product_profile_compiler.cmake`](../../Modules/ui/vivid/cmake/product_profile_compiler.cmake)、
  [`scene.cppm`](../../Modules/ui/vivid/core/scene.cppm)、
  [`stack_usage_gate.cmake`](../../Modules/ui/vivid/cmake/stack_usage_gate.cmake)

本文只约束 Vivid domain，不定义 Charm Core 或整机 RAM 预算。

## Resident RAM 范围

计入 Vivid 自有常驻状态：Scene/SoA kernel、payload/text/semantic/style pools、唯一 live DrawCmd buffer、layer
snapshot stores、compaction/executor/traversal workspace、theme/stylesheet/image registry 和启用 widget 的
保守 style reserve。应用级 PerfOverlay runtime、canvas/text profile counter 与 DrawCmd policy 等固定全局
状态同样计入，不能藏在未解释的 process global 中。

不计入 platform framebuffer/显存、只读 font/image resource、应用对象、任务栈、driver DMA buffer 和其它
domain memory。因此通过只证明 Vivid 在分配预算内装得下并留有 headroom。

## 双层准入

配置期使用平台无关保守上界：

```text
upper_bound_bytes + min_headroom_bytes <= budget_bytes
```

目标编译器再用真实 ABI `sizeof` 验证：

```text
target_abi_exact_bytes <= upper_bound_bytes
target_abi_exact_bytes + min_headroom_bytes <= budget_bytes
```

任一层失败都拒绝需要 admission 的 target。`FULL` 可以生成 profile 而不强制产品预算；PRODUCT/MCU
profile 必须显式提供 scene count、budget 和 headroom，不能依赖隐式默认。

`Scene` 的 exact profile 同时验证 live DrawCmd buffer 数量、snapshot/workspace、global style/resource 和
runtime diagnostic/policy 状态。configure model 为 runtime globals 保留独立保守上界；低估真实 ABI 是
独立硬失败，不能用增大产品 budget 掩盖。

SoA common table 以 node capacity 乘算，但每个 node 只保存强类型 8 位 `StylePatch` 槽索引；完整 patch 位于
`STYLE_PATCH_SLOT_CAP` 控制的固定容量稀疏池。FULL/MCU_MIN 默认取 `min(soa_max_nodes, 192)`，PRODUCT 必须在
profile 中显式声明该容量，且不得超过 255 或 node capacity。配置期分别计算 node 与 patch-slot 保守上界，不能把槽池
成本重新藏回逐节点常量。目标 ABI 的 `Scene`/`SoaKernel` exact profile 才是实际收益证据。

Semantic role、id、label 与 action mask 同样位于 `SEMANTIC_SLOT_CAP` 控制的固定容量稀疏池，common table
只保存独立强类型的 8 位槽索引。FULL/MCU_MIN 默认取 `min(soa_max_nodes, 64)`，PRODUCT 必须显式声明容量且
不得超过 255 或 node capacity。配置期按 16B/slot 加固定管理上界单列 semantic pool，不得把完整 semantic record 重新乘到
所有 node。profile fingerprint、typed config、static-memory manifest 与 admission JSON 必须消费同一容量真源。

common table 只保存一份当前 `Rect`；layout、paint culling 与 hit-test 共同消费它。不得为未获准的视觉越界能力
恢复逐节点 `paint_bounds` 副本。需要扩展绘制边界时，应按实际消费者引入固定容量、可裁剪且有 overflow evidence
的 decoration/effect 存储。

layout kind 与 label 双轴对齐不分别乘算三张 byte table，而是共享 1B/node 的显式 packed state。配置期
`soa_node_upper_bytes` 必须反映该固定节省，目标 ABI 的 `SoaKernel`/`Scene` 尺寸和 SoA 组合回归共同证明
实际布局、对齐与生命周期默认值没有因打包漂移。

node free-list link 与活动 payload slot 的生命周期互斥，必须复用同一 2B/node storage slot；payload pool
以 owner node index 拒绝旧 owner 访问或释放已复用槽。该 owner 表替换 pool generation 表，不增加 pool
容量乘数。`WidgetHandle` generation 仍负责公开 node identity，不得与内部 payload ownership 混为一层。

SoA 直接子节点集合与顺序以 `first_child` / `next_sibling` 链为唯一真相，不为每个 node 常驻重复的 16 位
`child_count`。首子节点的内部 `prev_sibling` 槽编码该链的 tail，公开 helper 隔离存储布局，使 append、detach
与逆序 traversal 保持 O(1)，并删除独立的 16 位 `last_child` 表。配置期 `soa_node_upper_bytes` 必须合计扣除
这 4B/node；保留的 `child_count()` 查询按 `soa_max_nodes` 有界派生，并在 debug 构建对损坏索引或 sibling
环断言。该节省必须由目标 ABI 的 `SoaKernel` / `Scene` exact profile 和 link/unlink/reparent/destroy 回归共同证明。

Visible、Enabled、Focusable、HitTest、ClipChildren 与 hover/press/focus 必须共享 1B/node runtime state，
且通过 helper 隔离位布局；节点存活由既有 `kind != None` 真源判断，不得恢复重复的 Used 位。仅由
SegmentedControl/TabView 消费的 underline presentation 存入对应 payload 的既有对齐空隙，payload 尺寸不得
增长。通用 style variant 不得重新进入 node 或 dense style table；逐实例视觉差异由已受容量管理的 style
class 或 local patch 表达，presentation 写入只能触发 paint dirty。

`StyleClassId` 使用 8 位存储，`STYLE_CLASS_MAX` 必须位于 `1..256`；无效值为 0，因此最大配置仍可表达
`1..255` 的全部有效 class。SoA common table 保持 O(1) dense class lookup，但不得为历史 16 位配置范围恢复
2B/node。该 ID 只属于进程内 UI 运行时，不是 wire、持久化或跨 image ABI。

槽池耗尽不得覆盖现有 patch 或静默丢弃证据：失败写入保持目标 node 无 patch，并设置 sticky
`style_patch_overflowed`、累加 allocation-fail。clear 与 node destroy 必须归还槽；主 Scene 的 overflow evidence
通过 `Scene::last_cmd_stats()` 进入 SoA CI 最终判定。独立池耗尽回归用于证明拒绝和槽复用，不代表产品正常路径
允许 overflow。

Semantic pool 使用相同拒绝原则：池满时新 entry 保持 `found=false`，但有效 node snapshot 仍返回原 handle；
`set_semantic_actions()` 对没有 entry 的 node 无副作用。clear 与 node destroy 必须归还槽，overflow/fail/peak/live
证据由 SoA CI 单独验证，主 Scene 的 `semantic_overflowed` 必须通过 `Scene::last_cmd_stats()` 保持可见且正常路径为零。

SoA layout、render、hit-test、focus 与 semantic 不分别乘算遍历数组。每个 `SoaKernel` 只拥有一套
`soa_max_nodes` 容量的共享 traversal workspace。正常 runtime 的 frame 固定为 52B；仅开启 Draw Detail
evidence 时才携带 `draw_scope_id` 并固定为 56B。配置期上界、manifest 与 admission JSON 必须消费同一
feature 值，不能让 PRODUCT 为关闭的 evidence 字段付费；目标 ABI profile 记录实际 frame 与 workspace 字节。
phase lease 禁止重入覆盖：冲突必须拒绝后进入的 traversal，并同时设置 sticky
`traversal_phase_conflicted` 与 `workspace_overflowed`。独立冲突回归只证明拒绝和释放恢复；产品正常路径要求
两项 evidence 均为零。

Draw Detail evidence 还会为每个 SoA node 增加 2B `draw_scope`。因此配置期 `soa_node_upper_bytes` 必须在正常
209B 与 evidence 211B 之间切换；frame 与 node 预算必须由同一 target feature 驱动，禁止只计一项。

## Product Profile 与 Envelope

PRODUCT 通过两层 DSL 分离：

- product profile：active widget kinds、payload capacities、SoA/Text/Semantic/Style/DrawCmd 工作集；
- target envelope：screen/pixel format、layer cache、scene count、RAM/headroom 和 stack limit。

每个 target 只能绑定一个规范化 profile 和一个 envelope；重复配置只有内容完全相同时才允许。旧手写
module/payload 白名单不构成兼容路径，迁移错误由 CMake source 定义。

`profile_fingerprint` 只表示规范化产品工作集；`target_fingerprint` 在其上增加硬件 envelope。等价内容
必须同 fingerprint，Host/MCU 使用同一产品 profile 时 profile fingerprint 应一致，硬件差异则必须反映在
target fingerprint。

## Evidence Artifacts

配置证据写入 target/profile 隔离的 generated 目录，覆盖：

- profile 与 target envelope；
- module closure/external requirements；
- static-memory admission；
- typed config、pool/feature source 与 stack source manifest。

具体文件名、JSON 字段和默认容量以 CMake/template/script 为准，不在本文复制。GNU-compatible MCU 可选
导出 absolute profile symbols，供最终 ELF/map evidence 读取 target ABI 值。板级 memory report 必须保持
total/upper/budget/headroom 算术一致，不能只抄 configure 上界。

## Stack Gate

PRODUCT/MCU 使用编译器 `.su` evidence 检查当前 closure 中每个 Vivid function 的单函数 frame。超过 target
limit 或出现 unbounded dynamic stack 直接失败。manifest 必须按当前 source closure 过滤，避免复用 build
目录时读取已退出 profile 的旧 `.su`。

该门不证明累计调用链峰值。共享 object workspace 只允许单 UI execution domain 串行使用；同一 workspace
上的并发/重入必须拒绝或由调用方隔离。产品任务栈仍需入口调用链分析或 runtime high-water evidence。

## 验证入口

- [`vivid_product_profile_compiler_smoke.ps1`](../../scripts/vivid_product_profile_compiler_smoke.ps1)
- [`vivid_static_memory_admission_smoke.ps1`](../../scripts/vivid_static_memory_admission_smoke.ps1)

两脚本覆盖 profile/envelope/closure/fingerprint、预算正负例和 stack source 污染。它们默认可能配置
`soa_demo/cmake-build-soa-ci`，该目录可超过 1 GiB；磁盘受限环境必须显式传入已批准的 `-BuildDir`，并在
验证后按工作区策略清理。脚本通过不替代最终产品 ELF/map、任务栈或真实板内存证据。
