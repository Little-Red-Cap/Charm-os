# Canonical World Manifests

> status: `supporting`
>
> scope: system compiler witness/compare 工具的 manifest 输入

[`minimal_kernel_runtime.world.json`](minimal_kernel_runtime.world.json) 使用
`system_compiler.canonical_world/v0` schema，列出 subject、contract references、witness plan 和 compare
questions。格式约束见
[`system_compiler.canonical_world.v0.schema.json`](../../../schemas/system_compiler.canonical_world.v0.schema.json)。

该 manifest 只组织最小内核 runtime/syscall/trap 的证据输入，不替代 Host/QEMU smoke，也不证明所列
contract 或 witness 已通过。实际消费方与默认路径以 `scripts/` 中的 runtime session/evidence bundle
入口为准。
