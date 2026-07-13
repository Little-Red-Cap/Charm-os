# Minimal Kernel Runtime Session Witness Inspect Compare Consumer v0

## 文档状态

- `status`: `supporting`
- `scope`: runtime session witness compare 的只读排序与 explain-hop 选择
- `authority`: 受 [`kernel_runtime_session_witness_v0.md`](kernel_runtime_session_witness_v0.md) 与
  [`explain_surface_v0.md`](explain_surface_v0.md) 约束

本文件定义 workflow 与 schema index 消费的局部诊断工具边界，不定义 Charm Core、runtime verdict
或新的 compare 语言。

## 输入与输出

consumer 只读取一份已经验证的 `minimal_kernel.runtime_session_witness_inspect_compare/v0` summary：

```text
validated inspect-compare summary
  -> compare consumer exporter
  -> ordered focus entries + explain hops
```

输出回答两个问题：

- 当前 drift 应先检查哪个最小 focus；
- 对该 focus 应先打开哪个已有 artifact，必要时按什么顺序回退。

consumer 不重新比较 baseline/candidate，不解析 host 或 QEMU 原始日志，也不重判 session、world compare
或 witness compare 的结果。

## 排序策略

当前 exporter 按以下局部策略组织 focus：

1. session result、status 或 failure domain 变化优先；
2. runtime facts regression 次之；
3. world-compare 与 witness-compare failure delta 保持为独立 focus；
4. summary violation 作为最后的 gate-facing focus；
5. 没有 actionable drift 时保留 steady-state focus。

每个 focus 只引用 compare summary 已经暴露的事实，并选择一个 preferred explain hop 与有限 fallback。
它不得创建新的 failure code、runtime fact 或 artifact verdict。

## 权威实现

- output schema：[`minimal_kernel.runtime_session_witness_inspect_compare_consumer.v0.schema.json`](../../schemas/minimal_kernel.runtime_session_witness_inspect_compare_consumer.v0.schema.json)
- input schema：[`minimal_kernel.runtime_session_witness_inspect_compare.v0.schema.json`](../../schemas/minimal_kernel.runtime_session_witness_inspect_compare.v0.schema.json)
- exporter：[`export_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py`](../../scripts/export_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py)
- validator：[`validate_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py`](../../scripts/validate_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py)
- sample：[`minimal_kernel.runtime_session_witness_inspect_compare_consumer.v0.sample.json`](../../schemas/examples/minimal_kernel.runtime_session_witness_inspect_compare_consumer.v0.sample.json)

字段、severity、focus 内容、artifact ref 和排序实现以 schema/exporter 为准。本文件不复制完整字段表。

## 失败语义

输入 summary 不可读取、结构不合法、source result 不是 `ok` 或无法产生 focus 时，exporter/validator
必须失败。生成了 report、check 或 summary 文件，不代表输入 drift 已被证明，也不代表 runtime 已运行。

consumer 产物固定为 JSON summary、Markdown report 和 text check；后两者是 summary 的人读投影，
不能作为独立事实来源。

## 验证入口

从仓库根目录运行聚合 smoke：

```powershell
./scripts/system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1
```

只读检查已有 summary：

```powershell
./scripts/inspect_minimal_kernel_runtime_session_witness_inspect_compare_consumer.ps1 `
  -Summary out/minimal-kernel-runtime-session-witness-inspect-compare-consumer/session-witness.inspect.compare.consumer.summary.json
```

其它 CI 参数和输出路径由脚本及 workflow 维护，本文件不复制命令矩阵。
