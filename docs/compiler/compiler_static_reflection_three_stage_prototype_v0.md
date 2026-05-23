# Compiler Static Reflection Three-Stage Prototype v0

本文承接 [`compiler_world_ir_first_landing_slice_v0.md`](compiler_world_ir_first_landing_slice_v0.md)，定义 Charm 使用 C++26 static reflection 的第一条最小落地路线。

核心判断：

```text
reflection belongs to the hosted extraction stage
firmware consumes lowered residue
```

也就是说，`<meta>` 是 compile-time world builder 的工具，不应在 v0 中直接进入 freestanding firmware TU。

## 1. 三段式流程

v0 先固定三段：

```text
hosted reflection extraction
  -> generated freestanding residue
  -> firmware freestanding consumption
```

### hosted reflection extraction

- 使用支持 `-std=c++26 -freflection` 的 GCC。
- 可以包含 `<meta>`。
- 可以读取 C++ source facts，例如 resource / pin / binding 声明。
- 只产出结构化事实或 generated residue，不直接成为 firmware artifact。

### generated freestanding residue

- 由 extraction 阶段生成。
- 只包含固件可消费的 constexpr table、enum、plain struct 或 metadata。
- 不包含 `<meta>`。
- 不依赖 hosted-only libstdc++ header。

### firmware freestanding consumption

- 使用 `-ffreestanding` 编译。
- 只 include generated residue。
- 不重新执行 reflection。
- 不把 firmware TU 变成 world owner。

## 2. 为什么不直接在 freestanding TU 使用 `<meta>`

当前 ARM cross 编译器的 reflection frontend 与 `<meta>` header 已可用，但 `<meta>` 实现仍会牵出 `string`、`optional` 等 hosted libstdc++ 依赖。

因此，v0 不把“让 `<meta>` 直接在 freestanding firmware TU 中可用”作为目标。更稳的路线是把 reflection 保持在 extraction 阶段，让固件只消费 lowering 后的残留物。

## 3. 最小证明

最小证明应包含两个正向编译：

- hosted/extraction TU：`#include <meta>`，并能通过 `std::meta` 静态断言。
- firmware/residue TU：`-ffreestanding` 编译，并只 include generated residue。

当前探针入口：

- [`../../scripts/compiler_static_reflection_three_stage_probe.ps1`](../../scripts/compiler_static_reflection_three_stage_probe.ps1)

## 4. 与 World Pipeline 的关系

这条三段式流程对应 pipeline contract 中的：

```text
extract / normalize
  -> lower
  -> firmware consumption
```

`<meta>` 支撑的是 extraction/normalization，不是 frozen world truth 本身。generated residue 属于 lowering surface，firmware 只消费它。

## 5. 非目标

本 v0 不做：

- 不新增 `World IR` schema。
- 不定义 `WorldIR.hpp`、`TopologyIR.hpp` 或 C++ IR 类型。
- 不接 LLVM/MLIR。
- 不实现完整 extractor。
- 不把 `<meta>` 引入 freestanding firmware TU。
- 不改变现有 witness、artifact report、world compare 或 runtime evidence 判决模型。
