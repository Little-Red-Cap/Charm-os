# Minimal Kernel Runtime Session Witness Inspect Compare Consumer v0

> `status`: `supporting`

该 consumer 只读取一份已验证的 runtime-session inspect/compare summary，将已有 drift 排序为 focus，
并为每个 focus 选择 preferred explain hop 与有限 fallback。它受
[`kernel_runtime_session_witness_v0.md`](kernel_runtime_session_witness_v0.md) 和
[`explain_surface_v0.md`](explain_surface_v0.md) 约束，不定义 Charm Core、runtime verdict 或新的 compare
语言。

```text
validated inspect-compare summary
  -> compare consumer
  -> ordered focus entries + explain hops
```

## 排序边界

排序依次关注 session result/status/failure-domain 变化、runtime fact regression、world/witness compare
failure delta 和 summary violation；没有 actionable drift 时保留 steady-state focus。

consumer 只能引用输入 summary 已暴露的事实。它不重新比较 baseline/candidate，不解析 Host/QEMU 原始日志，
也不创建 failure code、runtime fact 或 artifact verdict。

## 事实源与失败

输入输出 shape 由 `minimal_kernel.runtime_session_witness_inspect_compare*.v0` schema 定义；排序与 artifact
选择以
[`export_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py`](../../scripts/export_minimal_kernel_runtime_session_witness_inspect_compare_consumer.py)
为准。

输入不可读、结构无效、source result 非 `ok` 或无法产生 focus 时必须失败。JSON summary 是消费事实源；
Markdown report 与 text check 只是人读投影，文件生成也不证明 drift 或 runtime 运行。

## 验证

聚合入口为
[`system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1`](../../scripts/system_compiler_minimal_kernel_runtime_session_witness_inspect_compare_consumer_smoke.ps1)。
CLI 参数与输出路径由脚本维护。
