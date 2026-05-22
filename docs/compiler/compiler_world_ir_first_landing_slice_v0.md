# Compiler World IR First Landing Slice v0

本文定义 `Compiler World IR / Pipeline Contract v0` 下面的第一块可落地切片。

它不是完整 `World IR`，不是 LLVM/MLIR 计划，也不是新的 schema。它只回答一件事：

> **如果我们只先落一小块世界，应该落哪一块、它的边界是什么、怎样才算真的落地。**

## 1. 定位

第一块切片必须足够小，能让 pipeline contract、witness contract 和 lowering surface 彼此对上，而不要求把整个系统编译宇宙一次性讲完。

v0 的推荐方向是：

- 以一个窄的 resource/topology 切片为核心
- 绑定到当前已有的 minimal-kernel / ARMv7-A 证据链语义
- 保留 witness、compare、artifact report 与 runtime evidence 的回指

它的目标不是“先把 LLVM 接上”，而是先证明：

```text
source facts
  -> semantic world slice
  -> freeze
  -> lowering
  -> witness / compare / report
```

这一条窄链路是可成立的。

## 2. 切片范围

v0 先只承认下面这类对象属于第一切片的合理范围：

- declared board / profile / resource facts
- derived topology / route / legality facts
- frozen semantic identity for the slice
- lowered artifact surfaces
- witness / evidence / compare surfaces

不在第一切片里的内容包括：

- 全量 `World IR`
- 全量 canonical identity 体系
- observation import pass
- LLVM/MLIR dialect
- 全系统 compare brain

## 3. 落地标准

一块切片只有在同时满足下面条件时，才算“真的落地”：

- 这块切片能从声明事实一路走到冻结边界
- 这块切片能被 lower 成至少一个可见 artifact surface
- 这块切片能产出可回指的 witness 或 evidence
- compare / report 可以只消费导出 surfaces，而不重跑下层 brain
- 失败时可以明确指出是 ingress、extract、freeze、lower 还是 witness 出了问题

如果这些条件还不成立，那么它只是设计，不是落地。

## 4. 与 LLVM 的关系

LLVM 在这份切片里仍然只是未来的 `lower` 实现候选，不是主语。

第一切片必须先证明“semantic world slice 可以被稳定压扁”，然后 LLVM 才有资格成为那个压扁动作的一种实现方式。

换句话说：

- 先有切片
- 再有 lowering seam
- 最后才是 LLVM backend landing

## 5. 非目标

本 v0 不做：

- 不定义完整 `World IR` 数据结构
- 不定义 `WorldIR.hpp` / `TopologyIR.hpp` / `Node`
- 不定义 LLVM pass、MLIR dialect 或 codegen pipeline
- 不定义 canonical identity、fork id、hash、storage model
- 不定义 observation import pass
- 不改变现有 witness bundle、artifact report、world compare、bringup evidence 的字段或判决模型

## 6. 与上位文档的关系

- `compiler_world_ir_pipeline_contract_v0.md` 定义第一条窄管道。
- 本文定义那条管道里最先落地的切片。
- `compiler_lifecycle_summary_sidecar`、`archive manifest`、`freeze receipt` 仍然是消费与证明面，不替代切片本身。

如果未来要真的接 LLVM，就先在这块切片上赢，而不是在全世界上下注。
