# Compiler Hosted Reflection Extraction Surface v0

本文承接 [`compiler_static_reflection_three_stage_prototype_v0.md`](compiler_static_reflection_three_stage_prototype_v0.md)，定义 Charm 使用 C++26 static reflection 时的第一条 hosted extraction surface。

它不是完整 extractor 实现，也不是 `World IR`、schema、validator、LLVM/MLIR 或 codegen pipeline。它只冻结一条边界：

```text
<meta> belongs to hosted extraction
firmware consumes freestanding residue
```

## 1. 定位

`Compiler Hosted Reflection Extraction Surface v0` 是 `extract / normalize` 阶段的只读观察面。

它允许 hosted 工具 TU 使用支持 reflection 的编译器读取 C++ source facts，并把这些事实投影成结构化结果或 generated residue。它不拥有 world truth，也不把 firmware TU 变成 reflection runtime。

当前最小证明入口是：

- [`../../scripts/compiler_static_reflection_three_stage_probe.ps1`](../../scripts/compiler_static_reflection_three_stage_probe.ps1)

当前核心代码侧防线是：

- `semantic::static_reflection_enabled`
- `semantic::reflected_member_names_match_when_enabled(...)`
- `CHARM_ENABLE_HOSTED_REFLECTION_EXTRACTION`

这些入口共同表达同一条规则：默认不启用 `<meta>`；只有显式进入 hosted extraction 且编译器支持 reflection 时，reflection helper 才能工作。

## 2. 输入边界

v0 允许 hosted extraction surface 读取这些输入：

- C++ source facts，例如 `struct`、`enum`、`constexpr` 描述、resource descriptor、binding descriptor、field order。
- 显式为 extraction 准备的 semantic descriptors。
- 已有 contract / report / witness 文档中允许作为 source facts 的声明事实。
- 编译器 front-end 可见的 declaration surface。

v0 不允许它读取或猜测这些输入：

- QEMU 串口 raw log。
- runtime smoke 的原始 stdout。
- compare drift 输出背后的 lower-layer brain。
- 未声明为 source fact 的临时 build artifact。
- firmware binary 反推出来的隐式 truth。

如果 observation 或 runtime evidence 将来要影响 semantic world，必须走 `source fact proposal -> new world branch -> proof -> freeze`，不能由 hosted reflection surface 原地改写当前 world。

## 3. 输出边界

v0 允许 hosted extraction surface 产出这些结果：

- structured extracted facts。
- generated freestanding residue。
- plain `enum`、`constexpr table`、plain `struct`、metadata map。
- field-name / descriptor-order 这类可审计 projection。
- 人类可读的 extraction report 或 check text。

v0 输出必须满足：

- 不包含 `<meta>`。
- 不依赖 hosted-only libstdc++ header，例如 `string` / `optional`。
- 可被 `-ffreestanding` firmware TU 消费。
- 保留到 source fact 的可解释回指。
- 不把 generated residue 冒充成 semantic world truth 本体。

generated residue 是 lowering surface，不是 world owner。

## 4. 编译边界

hosted extraction TU 可以使用：

```text
-std=c++26
-freflection
#include <meta>
```

firmware residue consumer TU 必须保持：

```text
-ffreestanding
no <meta>
no hosted-only reflection dependency
```

v0 不允许：

- 全局打开 `-freflection`。
- 修改 ARM toolchain 文件来强行携带 `<meta>`。
- 在 freestanding firmware TU 中 include `<meta>`。
- 让普通 kernel/runtime modules 依赖 hosted reflection header。

这也是 `semantic.core` 默认把 `static_reflection_enabled` 置为 `false` 的原因。

## 5. 与 Semantic Core Pilot 的关系

`semantic.core` 提供的是通用 semantic language：

- `Result`
- `Readiness`
- `Verdict`
- `FailureDomain`
- typed `Ref<Tag>`
- `NamedValue<T>`
- `Projection<Descriptor, Field, Capacity>`
- concept probes
- reflection helper

这些结构可以被 freestanding kernel modules 消费。它们不是 hosted-only。

reflection helper 则必须由 `CHARM_ENABLE_HOSTED_REFLECTION_EXTRACTION=1` 与 compiler support 共同门控。kernel/runtime 模块可以写 reflection-aware static assertion，但必须通过 `semantic::reflected_member_names_match_when_enabled(...)` 这类 helper，使默认 freestanding 路径不触发 `<meta>`。

## 6. Extraction Authority

hosted reflection extraction surface 的 authority 是：

- 可以读取 source declarations。
- 可以形成 extracted candidate facts。
- 可以生成 residue 或 report。
- 可以指出 descriptor / field-order mismatch。

它没有这些 authority：

- 不能证明 legality。
- 不能执行 freeze。
- 不能改写 semantic identity。
- 不能修改 runtime verdict。
- 不能替代 witness / compare / ledger。
- 不能把 observed runtime behavior 直接写回 frozen world。

换句话说，它是 extraction surface，不是 verifier、freeze receipt、witness bundle 或 compare brain。

## 7. 最小落地标准

一条 hosted reflection extraction landing 在 v0 中只有同时满足下面条件，才算成立：

- hosted extraction TU 可以 include `<meta>` 并通过 reflection probe。
- generated residue 不包含 `<meta>`。
- freestanding consumer TU 可以只 include residue 并通过 `-ffreestanding` 编译。
- firmware 路径不需要 `-freflection`。
- `semantic::static_reflection_enabled` 在普通构建中保持 `false`。
- 任何 reflection field-name assertion 都由 semantic helper 门控。

当前最小 smoke 已由 [`../../scripts/compiler_static_reflection_three_stage_probe.ps1`](../../scripts/compiler_static_reflection_three_stage_probe.ps1) 证明这条三段式边界。

## 8. 非目标

本 v0 不做：

- 不实现完整 extractor。
- 不定义 `compiler_reflection_extract.summary.json`。
- 不新增 JSON schema、validator、compare verdict 或 smoke 家族。
- 不定义 `WorldIR.hpp`、`TopologyIR.hpp`、Node、canonical identity 或 storage model。
- 不接 LLVM/MLIR。
- 不接 CMake preset。
- 不全局打开 `-freflection`。
- 不把 `<meta>` 放进 freestanding firmware TU。
- 不改变 artifact report、runtime ledger、witness bundle、world compare 或 runtime evidence 的字段与判决模型。

## 9. 后续方向

如果要继续往实现推进，推荐顺序是：

1. 先做一个极小 hosted extractor，读取单个 source descriptor。
2. 生成一个 freestanding residue header。
3. 让一个 host smoke 和一个 freestanding compile probe 同时消费该 residue。
4. 再考虑是否需要 sidecar report。

不推荐下一步直接做完整 `World IR`、canonical identity、observation import pass 或 LLVM/MLIR dialect。
