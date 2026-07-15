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

计入 Vivid 自有常驻状态：Scene/SoA kernel、payload/text/style pools、唯一 live DrawCmd buffer、layer
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

## Product Profile 与 Envelope

PRODUCT 通过两层 DSL 分离：

- product profile：active widget kinds、payload capacities、SoA/Text/Style/DrawCmd 工作集；
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
